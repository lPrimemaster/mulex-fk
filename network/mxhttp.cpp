#include "../mxhttp.h"
#include "../mxsystem.h"
#include "../mxlogger.h"
#include "rpc.h"
#include <rpcspec.inl>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <libbase64.h>

#include <App.h>
#include <fstream>
#include <filesystem>

static us_listen_socket_t* _http_listen_socket = nullptr;
static std::thread* _http_thread;
static uWS::Loop* _ws_loop_thread;

namespace mulex
{
	struct WsRpcBridge;
} // namespace mulex

static std::mutex _mutex;
static std::set<uWS::WebSocket<false, true, mulex::WsRpcBridge>*> _active_ws_connections;

namespace mulex
{
	static std::string HttpReadFileFromDisk(const std::string& filepath)
	{
		std::ifstream file(filepath, std::ios::binary);
		if(!file.is_open())
		{
			LogError("[mxhttp] Failed to read requested file from disk.");
			return "";
		}

		std::string out;
		file.seekg(0, std::ios::end);	
		out.resize(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(out.data(), out.size());
		LogTrace("[mxhttp] Read file <%s> to serve via HTTP.", filepath.c_str());
		return out;
	}

	static std::string_view HttpGetMimeType(const std::string& urlPath)
	{
		std::string ext = std::filesystem::path(urlPath).extension().string();
		if (ext == ".html") return "text/html";
		if (ext == ".css")  return "text/css";
		if (ext == ".js")   return "application/javascript";
		if (ext == ".png")  return "image/png";
		if (ext == ".jpg")  return "image/jpeg";
		if (ext == ".jpeg") return "image/jpeg";
		return "text/plain";
	}

	template<bool SSL>
	static bool HttpServeFile(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
	{
		std::string urlPath = std::string(req->getUrl());

		if(urlPath == "/")
		{
			// Serve root as main index
			urlPath = "/index.html";
		}

		// Get the public root path
		static std::string serveDir = SysGetExperimentHome();
		if(serveDir.empty())
		{
			LogError("[mxhttp] Failed to serve files. SysGetExperimentHome() failed.");
			res->writeStatus("500 Internal Server Error")->end("<h1>System Experiment Not Running<h1>");
			return false;
		}

		std::string respath = serveDir + urlPath;
		if(!std::filesystem::is_regular_file(respath))
		{
			LogError("[mxhttp] Failed to serve files. Resource <%s> not found.", respath.c_str());
			res->writeStatus("404 Not Found")->end("<h1>404 - File Not Found<h1>");
			return false;
		}

		std::string data = HttpReadFileFromDisk(respath);
		std::string_view contentType = HttpGetMimeType(respath);

		res->writeHeader("Content-Type", contentType)
		   ->writeHeader("Content-Length", std::to_string(data.size()))
		   ->end(data);

		return true;
	}

	static std::vector<std::uint8_t> HttpByteVectorFromJsonB64(const char* args, std::uint64_t len)
	{
		std::vector<std::uint8_t> output;
		output.resize(len * 5); // 5 times the encoded string
		std::uint64_t wlen;
		
		if(base64_decode(args, len, reinterpret_cast<char*>(&output.front()), &wlen, 0) < 1)
		{
			LogError("[mxhttp] HttpByteVectorFromJsonB64: Failed to decode input.");
			return std::vector<std::uint8_t>();
		}

		// Dial the size back down
		output.resize(wlen);
		return output;
	}

	static rapidjson::Document HttpJsonFromByteVector(const std::vector<std::uint8_t>& arr)
	{
		rapidjson::Document d;
		d.SetObject();
		rapidjson::Value output(rapidjson::kArrayType);

		rapidjson::Document::AllocatorType& allocator = d.GetAllocator();
		for(const auto& byte : arr)
		{
			output.PushBack(rapidjson::Value().SetInt(byte), allocator);
		}
		d.AddMember("response", output, allocator);
		return d;
	}

	template<typename T>
	static T HttpTryGetEntry(const rapidjson::Document& d, const std::string& key, bool* error)
	{
		if(error) *error = false;
		if(d.HasMember(key.c_str()))
		{
			if constexpr(std::is_same_v<T, std::uint16_t>)
			{
				if(d[key.c_str()].GetType() != rapidjson::Type::kNumberType)
				{
					LogError("[mxhttp] HttpTryGetEntry: Found key <%s>. But is of incorrect type. Expected Number.");
					if(error) *error = true;
					return T();
				}
				return static_cast<std::uint16_t>(d[key.c_str()].GetInt());
			}
			else if constexpr(std::is_same_v<T, std::uint64_t>)
			{
				if(d[key.c_str()].GetType() != rapidjson::Type::kNumberType)
				{
					LogError("[mxhttp] HttpTryGetEntry: Found key <%s>. But is of incorrect type. Expected Number.");
					if(error) *error = true;
					return T();
				}
				return d[key.c_str()].GetUint64();
			}
			else if constexpr(std::is_same_v<T, bool>)
			{
				if(d[key.c_str()].GetType() != rapidjson::Type::kTrueType)
				{
					LogError("[mxhttp] HttpTryGetEntry: Found key <%s>. But is of incorrect type. Expected Boolean.");
					if(error) *error = true;
					return T();
				}
				return d[key.c_str()].GetBool();
			}
			else if constexpr(std::is_same_v<T, std::string>)
			{
				if(d[key.c_str()].GetType() != rapidjson::Type::kStringType)
				{
					LogError("[mxhttp] HttpTryGetEntry: Found key <%s>. But is of incorrect type. Expected String.");
					if(error) *error = true;
					return T();
				}
				return std::string(d[key.c_str()].GetString());
			}
			else
			{
				LogError("[mxhttp] HttpTryGetEntry: Could not TryGet type. This is an implementation issue.");
				if(error) *error = true;
				// HACK: (Cesar) Assume T() is valid
				return T();
			}
		}
		else
		{
			LogError("[mxhttp] HttpTryGetEntry: Could not find key <%s>.", key.c_str());
			if(error) *error = true;
			return T();
		}
	}

	static std::tuple<std::uint16_t, std::vector<std::uint8_t>, std::uint64_t, bool> HttpParseWSMessage(std::string_view message, bool* error)
	{
		rapidjson::Document d;
		// HACK: (Cesar) This is not ideal
		//				 However, We need to recheck how uWS handles the string_view output to, *maybe*, avoid copying
		d.Parse(std::string(message).c_str());

		if(error) *error = false;

		if(d.HasParseError())
		{
			LogError("[mxhttp] HttpParseWSMessage: Failed to parse RPC call.");
			if(error) *error = true;
			return std::make_tuple<std::uint16_t, std::vector<std::uint8_t>, std::uint64_t, bool>(0, {}, 0, true);
		}

		// If we want to use native types we should get someway of getting the RPC server side types
		// Multiple ways come to mind. For now we generate the same data on the frontend args array
	
		// NOTE: (Cesar) Use array of uint8 from frontend and cast it to std::vector<std::uint8_t>
		//				 The websocket already compresses the data
		
		// NOTE: (Cesar) To make it more space efficient we could send the data as uint64 instead
		std::string methodname = HttpTryGetEntry<std::string>(d, "method", error);
		if(error && *error)
		{
			return std::make_tuple<std::uint16_t, std::vector<std::uint8_t>, std::uint64_t, bool>(0, {}, 0, true);
		}
		std::uint16_t procedureid = RPCGetMethodId(methodname);
		if(procedureid == static_cast<std::uint16_t>(-1))
		{
			LogError("[mxhttp] HttpParseWSMessage: Received unknown rpc method name <%s>.", methodname.c_str());
			if(error) *error = true;
			return std::make_tuple<std::uint16_t, std::vector<std::uint8_t>, std::uint64_t, bool>(0, {}, 0, true);
		}

		// TryGet is not usable here
		std::vector<uint8_t> args;
		if(d.HasMember("args"))
		{
			if(d["args"].GetType() != rapidjson::Type::kStringType)
			{
				LogError("[mxhttp] HttpParseWSMessage: 'args' must be a base64 encoded string if any args exist.", methodname.c_str());
				if(error) *error = true;
				return std::make_tuple<std::uint16_t, std::vector<std::uint8_t>, std::uint64_t, bool>(0, {}, 0, true);
			}
			args = HttpByteVectorFromJsonB64(d["args"].GetString(), d["args"].GetStringLength());
			if(args.empty())
			{
				LogError("[mxhttp] HttpParseWSMessage: 'args' is empty. If the function call contains no arguments, omit this field.");
				if(error) *error = true;
				return std::make_tuple<std::uint16_t, std::vector<std::uint8_t>, std::uint64_t, bool>(0, {}, 0, true);
			}
		}

		std::uint64_t messageidws = HttpTryGetEntry<std::uint64_t>(d, "messageid", error);
		if(error && *error)
		{
			return std::make_tuple<std::uint16_t, std::vector<std::uint8_t>, std::uint64_t, bool>(0, {}, 0, true);
		}

		bool expectresult = HttpTryGetEntry<bool>(d, "response", error);
		if(error && *error)
		{
			return std::make_tuple<std::uint16_t, std::vector<std::uint8_t>, std::uint64_t, bool>(0, {}, 0, true);
		}

		LogTrace("[mxhttp] HttpParseWSMessage() OK.");
		return std::make_tuple(procedureid, args, messageidws, expectresult);
	}

	static std::string HttpMakeWSMessage(const std::vector<std::uint8_t>& ret, const std::string& status, const std::string& type, std::uint64_t messageid)
	{
		rapidjson::Document d;
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		if(!ret.empty())
		{
			d = HttpJsonFromByteVector(ret);
		}

		d.AddMember("status", rapidjson::StringRef(status.c_str(), status.size()), d.GetAllocator());
		d.AddMember("type", rapidjson::StringRef(type.c_str(), type.size()), d.GetAllocator());
		d.AddMember("messageid", messageid, d.GetAllocator());

		d.Accept(writer);

		LogTrace("[mxhttp] HttpMakeWSMessage() OK.");

		return buffer.GetString();
	}

	struct WsRpcBridge
	{
		std::unique_ptr<RPCClientThread> _local_rct;
		std::unique_ptr<EvtClientThread> _local_ect;
	};

	void HttpStartServer(std::uint16_t port)
	{
		_http_thread = new std::thread([port](){
			_ws_loop_thread = uWS::Loop::get(); // Only read is ok
			uWS::App().get("/*", [](auto* res, auto* req) {
				HttpServeFile(res, req);
			}).ws<WsRpcBridge>("/*", {
				.compression = uWS::CompressOptions(uWS::DEDICATED_COMPRESSOR_4KB | uWS::DEDICATED_COMPRESSOR),
				.maxPayloadLength = 100 * 1024 * 1024,
				.idleTimeout = 16,
				.maxBackpressure = 100 * 1024 * 1024,
				.closeOnBackpressureLimit = false,
				.resetIdleTimeoutOnSend = false,
				.sendPingsAutomatically = true,
				.upgrade = nullptr,
				
				.open = [](auto* ws) {
					{
						std::lock_guard<std::mutex> lock(_mutex);
						_active_ws_connections.insert(ws);
					}

					WsRpcBridge* bridge = ws->getUserData();

					// Initialize a local rpc client to push ws requests
					// FIXME: (Cesar) This client should not have an id (?)
					// NOTE: (Cesar) We can hack into the server rpc thread stack instead of sending a socket request
					// 				 This will however be harder to do for events I think
					// 				 I would say it is not worth the efort / problems if the local network call
					// 				 does not pose any performance problems in the future
					bridge->_local_rct = std::make_unique<RPCClientThread>("localhost");
					bridge->_local_ect = std::make_unique<EvtClientThread>("localhost");
					LogDebug("[mxhttp] New WS connection.");
				},
				.message = [](auto* ws, std::string_view message, uWS::OpCode opcode) {

					// TODO: (Cesar): Support events

					WsRpcBridge* bridge = ws->getUserData();

					LogTrace("[mxhttp] Received WS message <%s>.", std::string(ws->getRemoteAddressAsText()).c_str());

					// Parse the received data
					bool parse_error;
					const auto [procedureid, args, messageidws, exresult] = HttpParseWSMessage(message, &parse_error);
					if(parse_error)
					{
						// Parse error, ignore this message
						return;
					}

					if(exresult)
					{
						std::vector<std::uint8_t> result;
						// NOTE: (Cesar) If this gets expensive we move the call to another thread and defer send to ws
						// 				 Backpressure should handle this automatically via the uWS buffer
						bridge->_local_rct->callRaw(procedureid, args, &result);
						const std::string retmessage = HttpMakeWSMessage(result, "OK", "rpc", messageidws);
						ws->send(retmessage);
					}
					else
					{
						bridge->_local_rct->callRaw(procedureid, args, nullptr);
						const std::string retmessage = HttpMakeWSMessage(std::vector<std::uint8_t>(), "OK", "rpc", messageidws);
						ws->send(retmessage);
					}
				},
				.close = [](auto* ws, int code, std::string_view message) {
					WsRpcBridge* bridge = ws->getUserData();

					// Delete the local rpc/ect client bridges for this connection
					bridge->_local_rct.reset();
					bridge->_local_ect.reset();
					LogDebug("[mxhttp] Closing WS connection.");
				}
			}).listen(port, [port](auto* token) {
				if(token)
				{
					_http_listen_socket = token;
					LogMessage("[mxhttp] Started listening on port %d.", port);
					LogDebug("[mxhttp] HttpStartServer() OK.");
				}
				else
				{
					LogError("[mxhttp] Failed to listen on port %d.", port);
				}
			}).run();
		});
	}

	void HttpStopServer()
	{
		LogDebug("[mxhttp] Closing http server.");
		if(_http_listen_socket)
		{
			// Defer close all current connections (we don't want to wait on the browser)
			_ws_loop_thread->defer([]() {
				std::lock_guard<std::mutex> lock(_mutex); // Given defer this should not be needed
				for(auto* ws : _active_ws_connections)
				{
					ws->close();
				}
				_active_ws_connections.clear();
			});

			us_listen_socket_close(0, _http_listen_socket);
			_http_thread->join();
			delete _http_thread;
		}
	}
} // namespace mulex
