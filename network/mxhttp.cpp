#include "../mxhttp.h"
#include "../mxsystem.h"
#include "../mxlogger.h"
#include "PerMessageDeflate.h"
#include "rpc.h"
#include <rpcspec.inl>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <libbase64.h>

#include <App.h>
#include <fstream>
#include <filesystem>

#include <mxres.h>

static us_listen_socket_t* _http_listen_socket = nullptr;
static std::thread* _http_thread;
static uWS::Loop* _ws_loop_thread;

namespace mulex
{
	struct WsRpcBridge;
} // namespace mulex

struct HttpClientInfo
{
	mulex::mxstring<32> _ip;
	std::int64_t  _timestamp;
	std::uint64_t _identifier;
};

using UWSType = uWS::WebSocket<false, true, mulex::WsRpcBridge>;
static std::mutex _mutex;
static std::set<UWSType*> _active_ws_connections;
static std::unordered_map<UWSType*, HttpClientInfo> _active_clients_info;
static std::mutex _aci_lock;
// static std::unordered_map<std::string, std::set<UWSType*>> _active_ws_subscriptions;
static std::unique_ptr<mulex::SysFileWatcher> _plugins_watch;

namespace mulex
{
	struct WsRpcBridge
	{
		Experiment _local_experiment;
	};

	static bool HttpGenerateViteConfigFile()
	{
		std::string serveDir = SysGetExperimentHome();
		if(!std::filesystem::exists(serveDir + "/vite.config.ts"))
		{
			LogDebug("[mxhttp] Plugin <vite.config.ts> does not exist. Creating file...");
			SysWriteBinFile(serveDir + "/vite.config.ts", ResGetResource("vite.config.ts"));
		}
		else
		{
			LogTrace("[mxhttp] Plugin <vite.config.ts> found under experiment home.");
		}

		return true;
	}

	static bool HttpStartWatcherThread()
	{
		// We transpile with vite (via yarn build, must be installed on server-side)
		std::string serveDir = SysGetExperimentHome();
		if(serveDir.empty())
		{
#ifdef ONLINE_TRANSPILER
			LogError("[mxhttp] Failed to start the realtime transpiler thread. SysGetExperimentHome() failed.");
#else
			LogError("[mxhttp] Failed to start the realtime plugin watcher thread. SysGetExperimentHome() failed.");
#endif
			return false;
		}

#ifdef ONLINE_TRANSPILER
		if(!HttpGenerateViteConfigFile())
		{
			return false;
		}
#endif

		std::string pluginsDir = serveDir + "/plugins";


		if(!std::filesystem::is_directory(pluginsDir) && !std::filesystem::create_directory(pluginsDir))
		{
			LogError("[mxhttp] Failed to create/attach to user plugins directory.");
			return false;
		}

		if(!RdbFindEntryByName("/system/http/hotswap"))
		{
			RdbCreateValueDirect("/system/http/hotswap", RdbValueType::BOOL, 0, true);
		}

#ifdef ONLINE_TRANSPILER
		if(!RdbCreateValueDirect("/system/http/online_transpiler", RdbValueType::BOOL, 0, true))
		{
			RdbWriteValueDirect("/system/http/online_transpiler", true);
		}
#else
		if(!RdbCreateValueDirect("/system/http/online_transpiler", RdbValueType::BOOL, 0, false))
		{
			RdbWriteValueDirect("/system/http/online_transpiler", false);
		}
#endif

		_plugins_watch = std::make_unique<SysFileWatcher>(pluginsDir, [serveDir](const SysFileWatcher::FileOp op, const std::string& file) {
			if(RdbReadValueDirect("/system/http/hotswap").asType<bool>())
			{
#ifdef ONLINE_TRANSPILER
				std::string command = " yarn --cwd '";
				command += serveDir;
				command += "' build";
#endif
				std::string filename = std::filesystem::path(file).filename();
				switch(op) {
					case SysFileWatcher::FileOp::CREATED:
					{
						HttpRegisterUserPlugin(filename);
						[[fallthrough]];
					}
					case SysFileWatcher::FileOp::MODIFIED:
					{
#ifdef ONLINE_TRANSPILER
						std::thread([filename, command](){
#ifdef __linux__
							std::system(("ENTRY_FILE=./plugins/" + filename + command).c_str());
#else
							LogError("[mxhttp] Windows does not yet support frontend file hotswaping.");
#endif
						}).detach();
#endif
						break;
					}
					case SysFileWatcher::FileOp::DELETED:
					{
						std::string filename = std::filesystem::path(file).filename();
						HttpRemoveUserPlugin(filename);
						break;
					}
				}
			}
		});

		LogDebug("[mxhttp] Plugin watcher realtime thread OK.");
		return true;
	}

	static void HttpStopTranspilerThread()
	{
		if(_plugins_watch)
		{
			_plugins_watch.reset();
		}
	}

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
			if constexpr(std::is_same_v<T, std::uint8_t>)
			{
				if(d[key.c_str()].GetType() != rapidjson::Type::kNumberType)
				{
					LogError("[mxhttp] HttpTryGetEntry: Found key <%s>. But is of incorrect type. Expected Number.", key.c_str());
					if(error) *error = true;
					return T();
				}
				return static_cast<std::uint8_t>(d[key.c_str()].GetInt());
			}
			else if constexpr(std::is_same_v<T, std::uint16_t>)
			{
				if(d[key.c_str()].GetType() != rapidjson::Type::kNumberType)
				{
					LogError("[mxhttp] HttpTryGetEntry: Found key <%s>. But is of incorrect type. Expected Number.", key.c_str());
					if(error) *error = true;
					return T();
				}
				return static_cast<std::uint16_t>(d[key.c_str()].GetInt());
			}
			else if constexpr(std::is_same_v<T, std::uint64_t>)
			{
				if(d[key.c_str()].GetType() != rapidjson::Type::kNumberType)
				{
					LogError("[mxhttp] HttpTryGetEntry: Found key <%s>. But is of incorrect type. Expected Number.", key.c_str());
					if(error) *error = true;
					return T();
				}
				return d[key.c_str()].GetUint64();
			}
			else if constexpr(std::is_same_v<T, bool>)
			{
				if(d[key.c_str()].GetType() != rapidjson::Type::kTrueType && d[key.c_str()].GetType() != rapidjson::Type::kFalseType)
				{
					LogError("[mxhttp] HttpTryGetEntry: Found key <%s>. But is of incorrect type. Expected Boolean.", key.c_str());
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

	static rapidjson::Document HttpParseWSMessage(std::string_view message, bool* error)
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
			return d;
		}

		return d;
	}

	// tuple -> (opcode, eventname)
	static std::tuple<std::uint8_t, std::string> HttpGetEVTMessage(const rapidjson::Document& d, bool* error)
	{
		if(error) *error = false;

		std::string eventname = HttpTryGetEntry<std::string>(d, "event", error);
		if(error && *error)
		{
			return std::make_tuple<std::uint8_t, std::string>(0, {});
		}

		std::uint8_t opcode = HttpTryGetEntry<std::uint8_t>(d, "opcode", error);
		if(error && *error)
		{
			return std::make_tuple<std::uint8_t, std::string>(0, {});
		}

		return std::make_tuple(opcode, eventname);
	}

	static std::tuple<std::uint16_t, std::vector<std::uint8_t>, std::uint64_t, bool> HttpGetRPCMessage(const rapidjson::Document& d, bool* error)
	{
		// If we want to use native types we should get someway of getting the RPC server side types
		// Multiple ways come to mind. For now we generate the same data on the frontend args array
		
		if(error) *error = false;
	
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

		return std::make_tuple(procedureid, args, messageidws, expectresult);
	}

	static std::string HttpMakeWSRPCMessage(const std::vector<std::uint8_t>& ret, const std::string& status, std::uint64_t messageid)
	{
		rapidjson::Document d;
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		if(!ret.empty())
		{
			d = HttpJsonFromByteVector(ret);
		}
		else
		{
			d.SetObject();
		}

		d.AddMember("status", rapidjson::StringRef(status.c_str(), status.size()), d.GetAllocator());
		d.AddMember("type", rapidjson::StringRef("rpc"), d.GetAllocator());
		d.AddMember("messageid", messageid, d.GetAllocator());

		d.Accept(writer);

		return buffer.GetString();
	}

	static std::string HttpMakeWSEVTMessage(const std::vector<std::uint8_t>& ret, const std::string& eventname)
	{
		rapidjson::Document d;
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		if(!ret.empty())
		{
			d = HttpJsonFromByteVector(ret);
		}

		d.AddMember("event", rapidjson::StringRef(eventname.c_str(), eventname.size()), d.GetAllocator());
		d.AddMember("type", rapidjson::StringRef("evt"), d.GetAllocator());

		d.Accept(writer);

		LogTrace("[mxhttp] HttpMakeWSEVTMessage() OK.");

		return buffer.GetString();
	}

	static void HttpDeferCall(decltype(_active_ws_connections)::key_type ws, std::function<void(decltype(_active_ws_connections)::key_type)> func)
	{
		_ws_loop_thread->defer([&ws, func]() {
			func(ws);
		});
	}

	static void HttpDeferCallAll(std::function<void(decltype(_active_ws_connections)::key_type)> func)
	{
		_ws_loop_thread->defer([&func]() {
			std::lock_guard<std::mutex> lock(_mutex); // Given defer this should not be needed
			for(auto* ws : _active_ws_connections)
			{
				func(ws);
			}
		});
	}

	// uWS already supports publishing/subscribing from topics
	// Maybe we switch to this eventually??
	static void HttpSendEvent(UWSType* ws, const std::string& event, const std::uint8_t* data, std::uint64_t len)
	{
		std::vector<std::uint8_t> data_vector;

		// auto eventws = _active_ws_subscriptions.find(event);
		//
		// if(eventws == _active_ws_subscriptions.end())
		// {
		// 	LogError("[mxhttp] Cannot send event. No WS subscribed.");
		// 	LogError("[mxhttp] This is a bug.");
		// 	return;
		// }
		//
		// if(eventws->second.empty())
		// {
		// 	return;
		// }

		data_vector.resize(len);
		std::memcpy(data_vector.data(), data, len);

		// Move the data vector to the uWS loop thread
		HttpDeferCall(nullptr, [ws, data_vector, event](auto*) {
			const std::string message = HttpMakeWSEVTMessage(data_vector, event);
			LogTrace("[mxhttp] Sending event message to ws.");
			ws->send(message);
		});
	}

	static void HttpSubscribeEvent(UWSType* ws, const std::string& event)
	{
		// See if this event exists
		WsRpcBridge* bridge = ws->getUserData();
		auto& evtclient = bridge->_local_experiment._evt_client;
		std::uint16_t eid = evtclient->findEvent(event);
		if(eid == 0)
		{
			LogError("[mxhttp] Failed to subscribe to event. IPC event <%s> does not exist.", event.c_str());
			return;
		}

		evtclient->subscribe(event, [ws, event](auto* data, auto len, auto* userdata) {
			HttpSendEvent(ws, event, data, len);
		});

		LogTrace("[mxhttp] HttpSubscribeEvent() OK.");
	}

	static void HttpUnsubscribeEvent(UWSType* ws, const std::string& event)
	{
		WsRpcBridge* bridge = ws->getUserData();
		bridge->_local_experiment._evt_client->unsubscribe(event);
		LogTrace("[mxhttp] HttpUnsubscribeEvent() OK.");
	}

	static void HttpUnsubscribeEventAll(UWSType* ws)
	{
		WsRpcBridge* bridge = ws->getUserData();
		bridge->_local_experiment._evt_client->unsubscribeAll();
	}

	static std::uint64_t HttpGetNextClientId()
	{
		static std::atomic<std::uint64_t> ccid = 0;
		return ccid++;
	}

	static bool HttpCopyAppFiles()
	{
		// Do this everytime the server starts
		// in case some of the files were accidentaly modified by the user (e.g. during plugins creation/copy)
		std::string serveDir = SysGetExperimentHome();
		std::uint64_t ncopy = 0;

		SysWriteBinFile(serveDir + "/index.html", ResGetResource("index.html"));

		SysWriteBinFile(serveDir + "/index.js", ResGetResource("index.js"));
		SysWriteBinFile(serveDir + "/index.css", ResGetResource("index.css"));
		SysWriteBinFile(serveDir + "/favicon.ico", ResGetResource("favicon.ico"));

		return true;
	}

	bool HttpRegisterUserPlugin(const std::string& plugin)
	{
		LogDebug("[mxhttp] Registering user plugin <%s>.", plugin.c_str());
		return RdbCreateValueDirect(("/system/http/plugins/" + plugin).c_str(), RdbValueType::BOOL, 0, true);
	}

	void HttpRemoveUserPlugin(const std::string& plugin)
	{
		LogDebug("[mxhttp] Removing user plugin <%s>.", plugin.c_str());
		RdbDeleteValueDirect(("/system/http/plugins/" + plugin).c_str());
	}

	void HttpStartServer(std::uint16_t port, bool islocal, bool hotswap)
	{

		if(!HttpCopyAppFiles())
		{
			LogError("[mxhttp] Failed to copy app files to experiment cache dir.");
			return;
		}

		EvtRegister("mxhttp::newclient");
		EvtRegister("mxhttp::delclient");

		if(hotswap)
		{
			if(!HttpStartWatcherThread())
			{
				LogError("[mxhttp] Failed to start the plugin watcher thread.");
				return;
			}
		}
		else
		{
			LogWarning("[mxhttp] Running without hotswap. User plugins will not build automatically if online transpilation is on.");
		}

		_http_thread = new std::thread([port, islocal](){
			_ws_loop_thread = uWS::Loop::get(); // Only read is ok
			uWS::App().get("/*", [](auto* res, auto* req) {
				HttpServeFile(res, req);
			}).ws<WsRpcBridge>("/*", {
				.compression = uWS::CompressOptions(uWS::DEDICATED_COMPRESSOR_4KB | uWS::DEDICATED_COMPRESSOR),
				// .compression = uWS::DISABLED,
				.maxPayloadLength = 1024 * 1024 * 1024,
				.idleTimeout = 16,
				.maxBackpressure = 1024 * 1024 * 1024,
				.closeOnBackpressureLimit = false,
				.resetIdleTimeoutOnSend = false,
				.sendPingsAutomatically = true,
				.upgrade = nullptr,
				
				.open = [](auto* ws) {
					{
						std::lock_guard<std::mutex> lock(_mutex);
						_active_ws_connections.insert(ws);
					}

					std::string ip = std::string(ws->getRemoteAddressAsText());
					std::int64_t ts = SysGetCurrentTime();
					std::uint64_t id = HttpGetNextClientId();

					std::unique_lock lock_aci(_aci_lock);
					_active_clients_info[ws] = {
						._ip = ip,
						._timestamp = ts,
						._identifier = id
					};

					EvtEmit("mxhttp::newclient", reinterpret_cast<std::uint8_t*>(&_active_clients_info[ws]), sizeof(HttpClientInfo));

					WsRpcBridge* bridge = ws->getUserData();

					// Ghost client with custom id
					std::uint64_t custom_id = SysStringHash64(ip + std::to_string(ts) + std::to_string(id));

					// Initialize a local rpc client to push ws requests
					// NOTE: (Cesar) We can hack into the server rpc/evt threads stack instead of sending a socket request
					// 				 This will however be harder to do for events I think
					// 				 I would say it is not worth the efort / problems if the local network call
					// 				 does not pose any performance / latency problems in the future
					bridge->_local_experiment._rpc_client = std::make_unique<RPCClientThread>("localhost", RPC_PORT, custom_id);

					bridge->_local_experiment._evt_client = std::make_unique<EvtClientThread>(
						"localhost",
						&bridge->_local_experiment,
						EVT_PORT,
						true,
						custom_id
					);
					LogDebug("[mxhttp] New WS connection. [%x]", ws);
				},
				.message = [](auto* ws, std::string_view message, uWS::OpCode opcode) {

					WsRpcBridge* bridge = ws->getUserData();

					// Parse the received data
					bool parse_error;
					rapidjson::Document doc = HttpParseWSMessage(message, &parse_error);


					std::uint16_t type = HttpTryGetEntry<std::uint16_t>(doc, "type", &parse_error);
					if(parse_error)
					{
						return;
					}
					
					if(type == 0) // RPC call
					{
						const auto [procedureid, args, messageidws, exresult] = HttpGetRPCMessage(doc, &parse_error);
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
							bridge->_local_experiment._rpc_client->callRaw(procedureid, args, &result);
							const std::string retmessage = HttpMakeWSRPCMessage(result, "OK", messageidws);
							ws->send(retmessage);
						}
						else
						{
							bridge->_local_experiment._rpc_client->callRaw(procedureid, args, nullptr);
							const std::string retmessage = HttpMakeWSRPCMessage(std::vector<std::uint8_t>(), "OK", messageidws);
							ws->send(retmessage);
						}
					}
					else if(type == 1) // Evt subscription/unsubscription
					{
						const auto [opcode, event] = HttpGetEVTMessage(doc, &parse_error);
						if(parse_error)
						{
							// Parse error, ignore this message
							return;
						}

						if(opcode == 0) // subscribe
						{
							HttpSubscribeEvent(ws, event);
						}
						else if(opcode == 1) // unsubscribe
						{
							HttpUnsubscribeEvent(ws, event);
						}
						else
						{
							LogError("[mxhttp] Urecognized event opcode <%d>.", opcode);
						}
					}
					else
					{
						LogError("[mxhttp] Urecognized message type <%d>.", type);
					}
				},
				.close = [](auto* ws, int code, std::string_view message) {
					WsRpcBridge* bridge = ws->getUserData();

					// BUG: (Cesar) This is failing sometimes - the evtserver handles disconnected clients
					// 				But it would be nice to have some cleaning up when disconnect is clean
					// Unsubscribe from all events on this client if any
					// HttpUnsubscribeEventAll(ws);

					std::unique_lock lock_aci(_aci_lock);
					EvtEmit("mxhttp::delclient", reinterpret_cast<std::uint8_t*>(&_active_clients_info[ws]._identifier), sizeof(std::uint64_t));

					_active_clients_info.erase(ws);

					// Delete the local rpc/ect client bridges for this connection
					bridge->_local_experiment._rpc_client.reset();
					bridge->_local_experiment._evt_client.reset();
					LogDebug("[mxhttp] Closed WS connection. [%x]", ws);
				}
			}).listen(islocal ? "127.0.0.1" : "0.0.0.0", port, [port, islocal](auto* token) {
				if(token)
				{
					_http_listen_socket = token;
					LogMessage("[mxhttp] Started listening on port %d.", port);

					if(islocal)
					{
						LogMessage("[mxhttp] Loopback mode is active.");
					}

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

		HttpStopTranspilerThread();

		if(_http_listen_socket)
		{
			// Defer close all current connections (we don't want to wait on the browser)
			HttpDeferCallAll([](auto* ws) {
				ws->close();
			});

			us_listen_socket_close(0, _http_listen_socket);
			_http_thread->join();
			delete _http_thread;
		}
	}

	mulex::RPCGenericType HttpGetClients()
	{
		std::unique_lock lock_aci(_aci_lock);
		std::vector<HttpClientInfo> output;
		output.reserve(_active_clients_info.size());

		for(auto it = _active_clients_info.begin(); it != _active_clients_info.end(); it++)
		{
			output.push_back(it->second);
		}

		return output;
	}
} // namespace mulex
