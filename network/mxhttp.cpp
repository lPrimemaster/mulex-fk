#include "../mxhttp.h"
#include "../mxsystem.h"
#include "../mxlogger.h"
#include <App.h>
#include <fstream>
#include <filesystem>

static us_listen_socket_t* _http_listen_socket = nullptr;
static std::thread* _http_thread;

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

	void HttpStartServer(std::uint16_t port)
	{
		_http_thread = new std::thread([port](){
			uWS::App().get("/*", [](auto* res, auto* req) {
				HttpServeFile(res, req);
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
			us_listen_socket_close(0, _http_listen_socket);
			_http_thread->join();
			delete _http_thread;
		}
	}
} // namespace mulex
