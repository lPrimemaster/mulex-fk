#include "../mxhttp.h"
#include "../mxsystem.h"
#include "../mxlogger.h"
#include "PerMessageDeflate.h"
#include "WebSocket.h"
#include "rpc.h"
#include <rpcspec.inl>
#include <dbperms.inl>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <libbase64.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <App.h>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <random>

#include <mxres.h>

#include <tracy/Tracy.hpp>

static us_listen_socket_t* _http_listen_socket = nullptr;
static std::thread* _http_thread;
static uWS::Loop* _ws_loop_thread;
static std::string _hms_secret;

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
		std::string _ip;
		mulex::PdbPermissions _user_permissions;
		std::string _username;
	};

	static bool HttpStartWatcherThread()
	{
		ZoneScoped;
		// We transpile with vite (via yarn build, must be installed on server-side)
		std::string serveDir = SysGetExperimentHome();
		if(serveDir.empty())
		{
			LogError("[mxhttp] Failed to start the realtime plugin watcher thread. SysGetExperimentHome() failed.");
			return false;
		}

		std::string pluginsDir = serveDir + "/plugins";

		if(!std::filesystem::is_directory(pluginsDir) && !std::filesystem::create_directory(pluginsDir))
		{
			LogError("[mxhttp] Failed to create/attach to user plugins directory.");
			return false;
		}

		// Remove all of the plugins and re-add user might have deleted files when mxmain was down
		std::vector<RdbKeyName> prev_plugins = RdbListSubkeys("/system/http/plugins/");
		for(const auto pplugin : prev_plugins)
		{
			RdbDeleteValueDirect(pplugin);
		}

		_plugins_watch = std::make_unique<SysFileWatcher>(
			pluginsDir,
			[serveDir](const SysFileWatcher::FileOp op, const std::string& file, const std::int64_t timestamp) {
				std::string filename = std::filesystem::path(file).filename().string();
				bool is_plugin = std::filesystem::path(file).filename().extension().string() == ".js";
				
				if(!is_plugin) return;

				switch(op)
				{
					case SysFileWatcher::FileOp::CREATED:
					{
						HttpRegisterUserPlugin(filename, timestamp);
						break;
					}
					case SysFileWatcher::FileOp::MODIFIED:
					{
						HttpUpdateUserPlugin(filename, timestamp);
						break;
					}
					case SysFileWatcher::FileOp::DELETED:
					{
						HttpRemoveUserPlugin(filename);
						break;
					}
				}
			},
			1000, // 1s interval poll
			false // not recursive (ignore chunk fulder)
		);

		LogDebug("[mxhttp] Plugin watcher realtime thread OK.");
		return true;
	}

	static void HttpStopWatcherThread()
	{
		ZoneScoped;
		if(_plugins_watch)
		{
			_plugins_watch.reset();
		}
	}

	static std::string HttpReadFileFromDisk(const std::string& filepath)
	{
		ZoneScoped;
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
		ZoneScoped;
		std::string ext = std::filesystem::path(urlPath).extension().string();
		if (ext == ".html") return "text/html";
		if (ext == ".css")  return "text/css";
		if (ext == ".js")   return "application/javascript";
		if (ext == ".png")  return "image/png";
		if (ext == ".jpg")  return "image/jpeg";
		if (ext == ".jpeg") return "image/jpeg";
		if (ext == ".ico")  return "image/x-icon";
		if (ext == ".pdf")  return "application/pdf";
		if (ext == ".mp4")  return "video/mp4";
		return "text/plain";
	}

	static rapidjson::Document HttpParseJSON(std::string_view message, bool* error)
	{
		ZoneScoped;
		rapidjson::Document d;
		// HACK: (Cesar) This is not ideal
		//				 However, We need to recheck how uWS handles the string_view output to, *maybe*, avoid copying
		d.Parse(std::string(message).c_str());

		if(error) *error = false;

		if(d.HasParseError())
		{
			LogError("[mxhttp] HttpParseJSON: Failed to parse JSON.");
			if(error) *error = true;
			return d;
		}

		return d;
	}

	template<typename T>
	static T HttpTryGetEntry(const rapidjson::Document& d, const std::string& key, bool* error)
	{
		ZoneScoped;
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
			else if constexpr(std::is_same_v<T, std::int64_t>)
			{
				if(d[key.c_str()].GetType() != rapidjson::Type::kNumberType)
				{
					LogError("[mxhttp] HttpTryGetEntry: Found key <%s>. But is of incorrect type. Expected Number.", key.c_str());
					if(error) *error = true;
					return T();
				}
				return d[key.c_str()].GetInt64();
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

	template<bool SSL>
	static std::pair<bool, std::string> HttpCheckToken(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
	{
		std::string auth = std::string(req->getHeader("cookie"));

		std::uint64_t start = auth.find("token=");
		if(start == std::string::npos)
		{
			return { false, "" };
		}

		// NOTE: (Cesar) if count is npos then get the full string
		std::uint64_t count = auth.substr(start).find(";");

		std::string key = auth.substr(start, count);
		std::string token = key.substr(key.find("=") + 1);
		
		return HttpJWSVerify(token);
	}

	// WARN: (Cesar) These files are public
	//				 If someone changes them to hack something
	//				 Well... They are public...
	static bool HttpIsRoutePublic(const std::string& route)
	{
		return
			route == "/login.html"  		 ||
			route == "/login.js"    		 ||
			route == "/favicon.ico" 		 ||
			route == "/login.css"   		 ||
			route == "/logo.png"			 ||
			route == "/manifest.webmanifest" ||
			route == "/registerSW.js"		 ||
			route == "/sw.js"				 ||
			route.starts_with("/workbox-");
	}

	template<bool SSL>
	static bool HttpServeLoginPage(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
	{
		std::string urlPath = std::string(req->getUrl());

		if(urlPath == "/")
		{
			// Serve root as main index
			urlPath = "/login.html";
		}

		// Get the public root path
		static std::string serveDir = SysGetExperimentHome();
		if(serveDir.empty())
		{
			LogError("[mxhttp] Failed to serve files. SysGetExperimentHome() failed.");
			res->writeStatus("500 Internal Server Error")->end("<h1>System Experiment Not Running<h1>");
			return false;
		}

		// Only allowed files
		if(!HttpIsRoutePublic(urlPath))
		{
			// Redirect wrong routes to login
			urlPath = "/login.html";
			// res->writeStatus("401 Unauthorized")->end("Invalid credentials");
		}

		std::string respath = serveDir + urlPath;
		std::string data = HttpReadFileFromDisk(respath);
		std::string_view contentType = HttpGetMimeType(respath);

		res->writeHeader("Content-Type", contentType)
		   // ->writeHeader("Content-Length", std::to_string(data.size()))
		   ->end(data);

		return true;
	}

	template<bool SSL>
	static bool HttpServeFile(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
	{
		ZoneScoped;
		std::string urlPath = std::string(req->getUrl());

		if(urlPath == "/" || urlPath == "/login.html")
		{
			// Serve root as main index
			// Serve login.html as main index
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
			// If the file is not found then route back to index.html
			// solidjs router will figure out the route from the full url
			urlPath = "/index.html";
			respath = serveDir + urlPath;
		}

		std::string data = HttpReadFileFromDisk(respath);
		std::string_view contentType = HttpGetMimeType(respath);

		res->writeHeader("Content-Type", contentType)
		   // ->writeHeader("Content-Length", std::to_string(data.size()))
		   ->end(data);

		return true;
	}

	template<bool SSL>
	static void HttpHandleLogin(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
	{
		res->onData([res, body = std::make_shared<std::string>("")](std::string_view data, bool last) {
			body->append(data);

			if(last)
			{
				bool error;
				rapidjson::Document parsed = HttpParseJSON(*body, &error);
				if(error)
				{
					res->writeStatus("400 Bad Request")->end();
					return;
				}

				std::string username = HttpTryGetEntry<std::string>(parsed, "username", &error);
				std::string password = HttpTryGetEntry<std::string>(parsed, "password", &error);

				if(error)
				{
					res->writeStatus("400 Bad Request")->end();
					return;
				}

				auto credentials = HttpGetUserCredentials(username);
				if(!credentials.has_value())
				{
					res->writeStatus("401 Unauthorized")->end("Invalid credentials");
					return;
				}

				// Found username check for credentials
				auto [salt, pass_hash] = credentials.value();
				std::string cat_pass_salt = salt + password;
				std::vector<std::uint8_t> buffer(cat_pass_salt.size());
				std::memcpy(buffer.data(), cat_pass_salt.c_str(), cat_pass_salt.size());

				if(!(SysSHA256Hex(buffer) == pass_hash))
				{
					res->writeStatus("401 Unauthorized")->end("Invalid credentials");
					return;
				}

				// Username and password are correct
				// Issue JWS token (1 day expiration)
				std::string token = HttpJWSIssue(username, "mx-auth-server", 86400);
				res->writeHeader("Set-Cookie", "token=" + token + "; HttpOnly; Path=/; SameSite=Strict; Max-Age=86400;");
				res->writeHeader("Content-Type", "application/json")->end("{\"token\":\"" + token + "\"}");
			}
		});

		res->onAborted([]() {
			LogError("[mxhttp] Login request aborted.");
		});
	}

	template<bool SSL>
	static void HttpHandleLogout(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
	{
		res->onData([res](std::string_view, bool last) {
			if(last)
			{
				res->writeHeader("Set-Cookie", "token=deleted; HttpOnly; Path=/; SameSite=Strict; Max-Age=0;");
				res->end("Logged out");
			}
		});

		res->onAborted([]() {
			LogError("[mxhttp] Logout request aborted.");
		});
	}

	template<bool SSL>
	static void HttpHandlePublicRPC(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
	{
		res->onData([res, body = std::make_shared<std::string>("")](std::string_view data, bool last) {
			body->append(data);

			if(last)
			{
				bool error;
				rapidjson::Document parsed = HttpParseJSON(*body, &error);
				if(error)
				{
					res->writeStatus("400 Bad Request")->end();
					return;
				}

				std::string info = HttpTryGetEntry<std::string>(parsed, "info", &error);
				if(error || info != "expname")
				{
					res->writeStatus("400 Bad Request")->end();
					return;
				}

				std::string expname = SysGetExperimentName().c_str();

				res->writeHeader("Content-Type", "application/json")->end("{\"return\":\"" + expname + "\"}");
			}
		});

		res->onAborted([]() {
			LogError("[mxhttp] Login request aborted.");
		});
	}

	template<bool SSL>
	static void HttpHandleUserData(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
	{
		res->onData([res, req](std::string_view, bool last) {
			if(last)
			{
				auto [valid, username] = HttpCheckToken(res, req);
				if(valid)
				{
					res->writeHeader("Content-Type", "application/json")->end("{\"return\":\"" + username + "\"}");
				}
				else
				{
					res->writeStatus("401 Unauthorized")->end("Invalid credentials");
				}
			}
		});

		res->onAborted([]() {
			LogError("[mxhttp] Username request aborted.");
		});
	}

	static std::vector<std::uint8_t> HttpByteVectorFromJsonB64(const char* args, std::uint64_t len)
	{
		ZoneScoped;
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
		ZoneScoped;
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
		ZoneScoped;
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
		ZoneScoped;
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
		ZoneScoped;
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
		ZoneScoped;
		// {
		// 	std::lock_guard<std::mutex> lock(_mutex); // Given defer this should not be needed
		// 	if(_active_ws_connections.find(ws) == _active_ws_connections.end()) return;
		// }

		_ws_loop_thread->defer([ws, func]() {
			LogTrace("[mxhttp] Calling defer within ws thread.");
			func(ws);
		});
	}

	static void HttpDeferCallAll(std::function<void(decltype(_active_ws_connections)::key_type)> func)
	{
		ZoneScoped;
		std::lock_guard<std::mutex> lock(_mutex); // Given defer this should not be needed
		for(auto* ws : _active_ws_connections)
		{
			_ws_loop_thread->defer([&func, ws]() {
				func(ws);
			});
		}
	}

	static void HttpSendEvent(UWSType* ws, const std::string& event, const std::uint8_t* data, std::uint64_t len)
	{
		ZoneScoped;
		std::vector<std::uint8_t> data_vector;

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
		ZoneScoped;
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
		ZoneScoped;
		WsRpcBridge* bridge = ws->getUserData();
		bridge->_local_experiment._evt_client->unsubscribe(event);
		LogTrace("[mxhttp] HttpUnsubscribeEvent() OK.");
	}

	static void HttpUnsubscribeEventAll(UWSType* ws)
	{
		ZoneScoped;
		WsRpcBridge* bridge = ws->getUserData();
		bridge->_local_experiment._evt_client->unsubscribeAll();
	}

	static std::uint64_t HttpGetNextClientId()
	{
		ZoneScoped;
		static std::atomic<std::uint64_t> ccid = 0;
		return ccid++;
	}

	static bool HttpCopyAppFiles()
	{
		ZoneScoped;
		// Do this everytime the server starts
		// in case some of the files were accidentaly modified by the user (e.g. during plugins creation/copy)
		std::string serveDir = SysGetExperimentHome();

		ResGetAll();
		for(const auto& [name, data] : ResGetAll())
		{
			LogDebug("[mxhttp] Writing <%s>...", name.c_str());
			SysWriteBinFile(serveDir + "/" + name, data);
		}

		// SysWriteBinFile(serveDir + "/index.html", ResGetResource("index.html"));
		//
		// SysWriteBinFile(serveDir + "/index.js", ResGetResource("index.js"));
		// SysWriteBinFile(serveDir + "/index.css", ResGetResource("index.css"));
		// SysWriteBinFile(serveDir + "/favicon.ico", ResGetResource("favicon.ico"));

		return true;
	}

	template<bool SSL>
	static void HttpStartServerInternal(uWS::TemplatedApp<SSL>& app, std::uint16_t port, bool islocal)
	{
		ZoneScoped;
		// using TAWSB = uWS::TemplatedApp<SSL>::template WebSocketBehavior<WsRpcBridge>;

		// TODO: (Cesar) Automate this process
		app.post("/api/login", [](auto* res, auto* req) {
			HttpHandleLogin(res, req);
		}).post("/api/logout", [](auto* res, auto* req) {
			HttpHandleLogout(res, req);
		}).post("/api/public", [](auto* res, auto* req) {
			HttpHandlePublicRPC(res, req);
		}).post("/api/auth/username", [](auto* res, auto* req) {
			HttpHandleUserData(res, req);
		}).get("/*", [](auto* res, auto* req) {
			auto [valid, _] = HttpCheckToken(res, req);
			if(valid)
			{
				HttpServeFile(res, req);
			}
			else
			{
				HttpServeLoginPage(res, req);
			}
		}).template ws<WsRpcBridge>("/*", {
			.compression = uWS::CompressOptions(uWS::DEDICATED_COMPRESSOR_4KB | uWS::DEDICATED_COMPRESSOR),
			// .compression = uWS::DISABLED,
			.maxPayloadLength = 1024 * 1024 * 1024,
			.idleTimeout = 16,
			.maxBackpressure = 1024 * 1024 * 1024,
			.closeOnBackpressureLimit = false,
			.resetIdleTimeoutOnSend = false,
			.sendPingsAutomatically = true,
			.upgrade = [](auto* res, auto* req, auto* ctx) {
				WsRpcBridge b;
				// This gets triggered if we going through a proxy
				b._ip = std::string(req->getHeader("x-real-ip"));

				auto [valid, username] = HttpCheckToken(res, req);

				if(!valid)
				{
					res->writeStatus("401 Unauthorized")->end("Invalid token");
					return;
				}

				// Get the user role and therefore permissions
				std::string user_role = PdbGetUserRole(username);
				b._user_permissions = PdbGetUserPermissions(user_role);
				b._username = username;

				// Token is valid -> upgrade the connection
				res->template upgrade<WsRpcBridge>(
					std::move(b),
					req->getHeader("sec-websocket-key"),
					req->getHeader("sec-websocket-protocol"),
					req->getHeader("sec-websocket-extensions"),
					ctx	
				);
			},
			.open = [](auto* ws) {
				{
					std::lock_guard<std::mutex> lock(_mutex);
					_active_ws_connections.insert(ws);
				}

				WsRpcBridge* bridge = ws->getUserData();

				std::string ip = bridge->_ip.empty() ? std::string(ws->getRemoteAddressAsText()) : bridge->_ip;
				std::int64_t ts = SysGetCurrentTime();
				std::uint64_t id = HttpGetNextClientId();

				std::unique_lock lock_aci(_aci_lock);
				_active_clients_info[ws] = {
					._ip = ip,
					._timestamp = ts,
					._identifier = id
				};

				EvtEmit("mxhttp::newclient", reinterpret_cast<std::uint8_t*>(&_active_clients_info[ws]), sizeof(HttpClientInfo));


				// Ghost client with custom id
				std::uint64_t custom_id = SysStringHash64(ip + std::to_string(ts) + std::to_string(id));

				// Initialize a local rpc client to push ws requests
				// NOTE: (Cesar) We can hack into the server rpc/evt threads stack instead of sending a socket request
				// 				 This will however be harder to do for events I think
				// 				 I would say it is not worth the efort / problems if the local network call
				// 				 does not pose any performance / latency problems in the future
				bridge->_local_experiment._rpc_client = std::make_unique<RPCClientThread>(
					"localhost",
					RPC_PORT,
					custom_id,
					bridge->_username
				);

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
				rapidjson::Document doc = HttpParseJSON(message, &parse_error);


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

					// TODO: (Cesar) What if we update the user permissions mid session?
					if(!PdbCheckMethodPermissions(procedureid, bridge->_user_permissions))
					{
						const std::string retmessage = HttpMakeWSRPCMessage(std::vector<std::uint8_t>(), "NO_PERM", messageidws);
						ws->send(retmessage);
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

				{
					std::lock_guard<std::mutex> lock(_mutex);
					_active_ws_connections.erase(ws);
				}

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
	}

	bool HttpRegisterUserPlugin(const std::string& plugin, std::int64_t timestamp)
	{
		ZoneScoped;
		LogDebug("[mxhttp] Registering user plugin <%s>.", plugin.c_str());
		return RdbCreateValueDirect(("/system/http/plugins/" + plugin).c_str(), RdbValueType::INT64, 0, timestamp);
	}

	bool HttpUpdateUserPlugin(const std::string& plugin, std::int64_t timestamp)
	{
		ZoneScoped;
		RdbKeyName key = ("/system/http/plugins/" + plugin).c_str();

		if(!RdbValueExists(key))
		{
			LogError("[mxhttp] Cannot update unknown user plugin <%s>.", plugin.c_str());
			return false;
		}

		LogDebug("[mxhttp] Updating user plugin <%s>.", plugin.c_str());
		RdbWriteValueDirect(key, timestamp);
		return true;
	}

	void HttpRemoveUserPlugin(const std::string& plugin)
	{
		ZoneScoped;
		LogDebug("[mxhttp] Removing user plugin <%s>.", plugin.c_str());
		RdbDeleteValueDirect(("/system/http/plugins/" + plugin).c_str());
	}

	void HttpStartServer(std::uint16_t port, bool islocal)
	{
		ZoneScoped;

		if(!HttpCopyAppFiles())
		{
			LogError("[mxhttp] Failed to copy app files to experiment cache dir.");
			return;
		}

		EvtRegister("mxhttp::newclient");
		EvtRegister("mxhttp::delclient");

		if(!HttpStartWatcherThread())
		{
			LogError("[mxhttp] Failed to start the plugin watcher thread.");
			return;
		}

		HttpInitUsersPdb();

		_http_thread = new std::thread([port, islocal](){
			_ws_loop_thread = uWS::Loop::get(); // Only read is ok

			auto app = uWS::App();
			HttpStartServerInternal(app, port, islocal);

			LogDebug("[mxhttp] Server graceful shutdown.");
		});
	}

	void HttpStopServer()
	{
		ZoneScoped;
		LogDebug("[mxhttp] Closing http server.");

		HttpStopWatcherThread();

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
		ZoneScoped;
		std::unique_lock lock_aci(_aci_lock);
		std::vector<HttpClientInfo> output;
		output.reserve(_active_clients_info.size());

		for(auto it = _active_clients_info.begin(); it != _active_clients_info.end(); it++)
		{
			output.push_back(it->second);
		}

		return output;
	}

	static std::string HttpBase64URLEncode(const std::string& data)
	{
		std::string buffer;

		// ROUND to ceil multiple of 4 and add 4 for safety
		std::uint64_t size = ((static_cast<std::uint64_t>(std::ceil((4.0/3.0) * data.size()) + 3)) & ~3) + 4;
		buffer.resize(size);
		base64_encode(data.c_str(), data.size(), buffer.data(), &size, 0);
		buffer.resize(size);

		std::replace(buffer.begin(), buffer.end(), '+', '-');
		std::replace(buffer.begin(), buffer.end(), '/', '_');

		while(!buffer.empty() && buffer.back() == '=')
		{
			buffer.pop_back();
		}

		return buffer;
	}

	static std::string HttpBase64URLDecode(const std::string& data)
	{
		std::string buffer = data;
		std::replace(buffer.begin(), buffer.end(), '-', '+');
		std::replace(buffer.begin(), buffer.end(), '_', '/');

		while(buffer.length() % 4 != 0)
		{
			buffer.push_back('=');
		}

		std::string output;
		std::uint64_t size = 5 * buffer.size();
		output.resize(size);

		if(base64_decode(buffer.data(), buffer.size(), output.data(), &size, 0) < 1)
		{
			LogError("[mxhttp] HttpBase64URLDecode: Failed to decode input.");
			return "";
		}

		output.resize(size);

		return output;
	}

	static std::string HttpHMS256(const std::string& data, const std::string& key)
	{
		std::uint8_t result[EVP_MAX_MD_SIZE];
		std::uint32_t len;
		if(HMAC(EVP_sha256(), key.c_str(), key.size(), reinterpret_cast<const std::uint8_t*>(data.c_str()), data.size(), result, &len) == nullptr)
		{
			LogError("[mxhttp] HttpHMS256: Signature generation failed.");
			return "";
		}
		return std::string(reinterpret_cast<char*>(result), len);
	}

	static std::string HttpHandleSecretKey()
	{
		// Check if there is a key at the cache
		if(!_hms_secret.empty())
		{
			return _hms_secret;
		}

		const std::string pcache = std::string(SysGetCachePrivateDir());
		if(pcache.empty())
		{
			LogError("[mxhttp] HttpHandleSecretKey: Failed to open private cache dir.");
			return "";
		}

		const std::string path = pcache + "/jws.key";
		if(std::filesystem::is_regular_file(path))
		{
			std::vector<std::uint8_t> key = SysReadBinFile(path);
			LogDebug("[mxhttp] Found jws.key. Using as secret...");
			_hms_secret = reinterpret_cast<char*>(key.data());
			return _hms_secret;
		}

		// Generate random key and write it to <jws.key> file
		std::string secret = SysGenerateSecureRandom256Hex();
		if(secret.empty())
		{
			LogError("[mxhttp] HttpHandleSecretKey: Failed to generate JWS key.");
			return "";
		}

		std::vector<std::uint8_t> data;
		data.resize(secret.size() + 1);
		std::memcpy(data.data(), secret.c_str(), secret.size() + 1);
		SysWriteBinFile(path, data);

		_hms_secret = secret;

		return secret;
	}

	std::string HttpJWSIssue(const std::string& sub, const std::string& iss, std::int64_t exp)
	{
		rapidjson::Document header;
		rapidjson::Document::AllocatorType& allocator = header.GetAllocator();
		header.SetObject();
		header.AddMember("alg", "HS256", allocator);
		header.AddMember("typ", "JWT", allocator);

		rapidjson::Document payload;
		payload.SetObject();
		std::int64_t now = SysGetCurrentTime() / 1000;
		payload.AddMember("sub", rapidjson::StringRef(sub.c_str(), sub.size()), allocator);
		payload.AddMember("iss", rapidjson::StringRef(iss.c_str(), iss.size()), allocator);
		payload.AddMember("exp", now + exp, allocator);

		// Encode header and payload
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		
		header.Accept(writer);
		std::string b64url_header = HttpBase64URLEncode(buffer.GetString());

		buffer.Clear();
		writer.Reset(buffer);

		payload.Accept(writer);
		std::string b64url_payload = HttpBase64URLEncode(buffer.GetString());

		const std::string data = b64url_header + "." + b64url_payload;

		// Sign
		const std::string signature = HttpHMS256(data, HttpHandleSecretKey());
		
		// Encode the signature
		const std::string b64url_signature = HttpBase64URLEncode(signature);

		return data + "." + b64url_signature;
	}

	std::pair<bool, std::string> HttpJWSVerify(const std::string& token)
	{
		// Split token on '.'
		std::vector<std::string> split;
		split.reserve(3);
		std::stringstream ss(token);
		std::string tk;
		while(std::getline(ss, tk, '.')) split.push_back(tk);
		if(split.size() != 3)
		{
			LogError("[mxhttp] HttpJWSVerify: Invalid token.");
			return { false, "" };
		}

		std::string header_str  = HttpBase64URLDecode(split[0]);
		std::string payload_str = HttpBase64URLDecode(split[1]);
		std::string signature   = HttpBase64URLDecode(split[2]);

		bool error;
		rapidjson::Document payload = HttpParseJSON(payload_str, &error);
		if(error)
		{
			LogError("[mxhttp] HttpJWSVerify: Invalid token.");
			return { false, "" };
		}

		std::string username = HttpTryGetEntry<std::string>(payload, "sub", &error);
		if(error)
		{
			LogError("[mxhttp] HttpJWSVerify: Invalid token.");
			return { false, "" };
		}

		std::int64_t expiration = HttpTryGetEntry<std::int64_t>(payload, "exp", &error);
		if(error || SysGetCurrentTime() / 1000 > expiration)
		{
			LogError("[mxhttp] HttpJWSVerify: Invalid token.");
			return { false, "" };
		}

		std::string issuer = HttpTryGetEntry<std::string>(payload, "iss", &error);
		if(error || issuer != "mx-auth-server")
		{
			LogError("[mxhttp] HttpJWSVerify: Invalid token.");
			return { false, "" };
		}

		const std::string signature_check = HttpHMS256(split[0] + "." + split[1], HttpHandleSecretKey());

		if(signature_check != signature)
		{
			LogError("[mxhttp] HttpJWSVerify: Invalid token.");
			return { false, "" };
		}

		return { true, username };
	}

	// NOTE: (Cesar) This is not meant to generate safe passwords
	static std::string HttpGenerateRandomPassword()
	{
		static constexpr std::uint64_t PASS_LEN = 16;
		static constexpr std::string_view ALPHANUM = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

		std::string output;
		std::random_device dev;
		std::mt19937 rng(dev());
		std::uniform_int_distribution<std::mt19937::result_type> dist(0, ALPHANUM.size() - 1);

		for(int i = 0; i < PASS_LEN; i++)
		{
			output += ALPHANUM[dist(rng)];
		}
		
		return output;
	}

	void HttpInitUsersPdb()
	{
		if(!PdbTableExists("users"))
		{
			// User table does not exist so probably the entire database does not either
			LogDebug("[mxhttp] Creating users database...");
			PdbSetupUserDatabase();

			const static std::vector<PdbValueType> types = {
				PdbValueType::NIL,
				PdbValueType::STRING,
				PdbValueType::STRING,
				PdbValueType::STRING,
				PdbValueType::INT32
			};

#ifndef CREATE_DEV_USER
			std::string random_salt = SysGenerateSecureRandom256Hex();
			std::string random_pass = HttpGenerateRandomPassword();
			std::string cat_pass_salt = random_salt + random_pass;
			std::vector<std::uint8_t> buffer(cat_pass_salt.size());
			std::memcpy(buffer.data(), cat_pass_salt.c_str(), cat_pass_salt.size());
			std::string hash = SysSHA256Hex(buffer);

			std::vector<std::uint8_t> data = SysPackArguments(
				PdbString("admin"),
				PdbString(random_salt),
				PdbString(hash),
				std::int32_t(1) // sysadmin role
			);
			PdbWriteTable("INSERT INTO users (id, username, salt, passhash, role_id) VALUES (?, ?, ?, ?, ?);", types, data);

			LogMessage("==============================================");
			LogMessage("==============================================");
			LogMessage("[mxhttp] Default admin created.");
			LogMessage("[mxhttp] username: admin");
			LogMessage("[mxhttp] password: %s", random_pass.c_str());
			LogWarning("[mxhttp] Save these credentials!");
			LogWarning("[mxhttp] They are required for first access.");
			LogMessage("==============================================");
			LogMessage("==============================================");
#else
			std::string random_salt = SysGenerateSecureRandom256Hex();
			std::string cat_pass_salt = random_salt + "dev";
			std::vector<std::uint8_t> buffer(cat_pass_salt.size());
			std::memcpy(buffer.data(), cat_pass_salt.c_str(), cat_pass_salt.size());
			std::string hash = SysSHA256Hex(buffer);

			std::vector<std::uint8_t> data = SysPackArguments(
				PdbString("dev"),
				PdbString(random_salt),
				PdbString(hash),
				std::int32_t(1) // sysadmin role
			);

			PdbWriteTable("INSERT INTO users (id, username, salt, passhash, role_id) VALUES (?, ?, ?, ?, ?);", types, data);
			LogMessage("==============================================");
			LogMessage("==============================================");
			LogMessage("[mxhttp] Default dev created.");
			LogMessage("[mxhttp] username: dev");
			LogMessage("[mxhttp] password: dev");
			LogWarning("[mxhttp] Credentials for development only.");
			LogMessage("==============================================");
			LogMessage("==============================================");
#endif
		}
	}

	std::optional<std::pair<std::string, std::string>> HttpGetUserCredentials(const std::string& username)
	{
		const static std::vector<PdbValueType> types = {
			PdbValueType::STRING,
			PdbValueType::STRING
		};

		PdbAccessLocal pdb;

		auto reader = pdb.getReader<PdbString, PdbString>("users", {"salt", "passhash"});
		auto credentials = reader("WHERE username = \"" + username + "\"");

		if(credentials.empty())
		{
			LogMessage("[mxhttp] Login attempt with username <%s> failed. Not registered.", username.c_str());
			return std::nullopt;
		}

		auto [salt, passhash] = credentials[0];
		return std::make_pair(salt.c_str(), passhash.c_str());
	}
} // namespace mulex
