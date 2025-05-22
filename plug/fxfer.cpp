#include "plug.h"
#include "../network/socket.h"
#include "../mxlogger.h"
#include "../mxsystem.h"
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <numeric>
#include <filesystem>
#include <ranges>
#include <thread>

static mulex::Socket _server_socket;
static std::unique_ptr<std::thread> _server_thread;
static std::atomic<bool> _server_run;

namespace mulex
{
	bool PlugFSInitialize()
	{
		_server_socket = SocketInit();
		SocketBindListen(_server_socket, FXFER_PORT);
		if(!SocketSetNonBlocking(_server_socket))
		{
			LogError("Failed to set listen socket to non blocking mode.");
			SocketClose(_server_socket);
			return false;
		}

		return true;
	}

	static void PlugFSSendData(const Socket& socket, const std::vector<std::uint8_t>& data)
	{
		SocketSendBytes(socket, const_cast<std::uint8_t*>(data.data()), data.size());
	}

	static std::optional<std::vector<std::uint8_t>> PlugFSRecvTimeout(const Socket& client, std::uint64_t size)
	{
		std::vector<std::uint8_t> buffer(size);
		std::uint64_t rlen, tsize = 0;
		const std::int64_t ms = SysGetCurrentTime();
		while(tsize < size)
		{
			SocketResult res = SocketRecvBytes(client, &buffer[tsize], size - tsize, &rlen);
			tsize += rlen;

			if(res == SocketResult::ERROR || res == SocketResult::DISCONNECT)
			{
				LogError("[mxplugfs] Could not process request.");
				return std::nullopt;
			}

			if((SysGetCurrentTime() - ms >= FXFER_TIMEOUT))
			{
				return std::nullopt;
			}

			std::this_thread::sleep_for(std::chrono::microseconds(50));
		}

		return buffer;
	}

	static std::optional<std::string> PlugFSReceiveExperimentName(const Socket& client)
	{
		auto expname_buffer = PlugFSRecvTimeout(client, sizeof(mxstring<512>));
		if(!expname_buffer.has_value())
		{
			return std::nullopt;
		}

		mxstring<512> expname;
		std::memcpy(&expname, expname_buffer.value().data(), sizeof(mxstring<512>));
		return expname.c_str();
	}

	static bool PlugFSCheckExperimentExists(const std::string& exp)
	{
		const std::string exp_dir = std::string(SysGetCacheDir()) + "/" + exp;
		if(!std::filesystem::is_directory(exp_dir))
		{
			LogError("[mxplugfs] The specified experiment could no be found under mxcache.");
			LogError("[mxplugfs] Looked under <%s>.", exp_dir.c_str());
			return false;
		}

		if(!std::filesystem::is_directory(exp_dir + "/plugins"))
		{
			LogError("[mxplugfs] Found experiment directory but could not find /plugins directory inside.");
			LogError("[mxplugfs] Looked under <%s>.", exp_dir.c_str());
			return false;
		}

		return true;
	}

	static std::string PlugFSGetExperimentPluginsDir(const std::string& exp)
	{
		return std::string(SysGetCacheDir()) + "/" + exp + "/plugins";
	}

	static std::optional<PlugFSHeader> PlugFSReceiveHeader(const Socket& client)
	{
		PlugFSHeader header;
		auto xfer_size = PlugFSRecvTimeout(client, sizeof(std::uint64_t));
		if(!xfer_size.has_value())
		{
			LogError("[mxplugfs] Failed to receive xfer total size.");
			return std::nullopt;
		}

		std::uint64_t to_read = *reinterpret_cast<std::uint64_t*>(xfer_size.value().data());

		auto metadata = PlugFSRecvTimeout(client, to_read);
		if(!metadata.has_value())
		{
			LogError("[mxplugfs] Failed to receive metadata.");
			return std::nullopt;
		}

		// At this point metadata_buffer.size() is guaranteed to be correct
		std::vector<std::uint8_t> metadata_buffer = metadata.value();
		std::uint64_t count = metadata_buffer.size() / sizeof(PlugFSFileMeta);
		std::vector<PlugFSFileMeta> meta_vec(count);
		std::memcpy(meta_vec.data(), metadata_buffer.data(), metadata_buffer.size());

		header._xfer_size = to_read;
		header._filemeta = meta_vec;

		return header;
	}

	static std::pair<std::vector<PlugFSFileMeta>, std::vector<PlugFSFileData>> PlugFSCreateMetadataAndData(const std::string& dir)
	{
		std::vector<PlugFSFileMeta> meta;
		std::vector<PlugFSFileData> data;

		// NOTE: (César) clock_cast is not yet available under GCC for C++20
		// TODO: (César) This function has multiple copies all over the code, refactor
		auto to_systime = [](const std::filesystem::file_time_type& time) -> std::int64_t {
			auto delta = std::filesystem::file_time_type::clock::now().time_since_epoch() - time.time_since_epoch();
			auto sys_file_time = std::chrono::system_clock::now().time_since_epoch() - delta;
			return std::chrono::duration_cast<std::chrono::milliseconds>(sys_file_time).count();
		};

		for(const auto& file : std::filesystem::recursive_directory_iterator(dir))
		{
			const std::string& file_path = file.path().string();
			const auto& last_write_ts = std::filesystem::last_write_time(file);

			if(file.is_regular_file())
			{
				auto contents = SysReadBinFile(file_path);
				const std::string& relative_path = std::filesystem::relative(file.path(), dir).string();

				LogDebug("[mxplug] Packing file <%s> with size <%u>.", relative_path.c_str(), contents.size());

				meta.push_back(PlugFSFileMeta {
					relative_path,
					contents.size(),
					to_systime(last_write_ts)
				});

				data.push_back(contents);
			}
		}

		return std::make_pair(meta, data);
	}

	// TODO: Use a checksum on the entire file to check integrity
	static std::optional<std::vector<PlugFSFileData>> PlugFSReceiveFiles(const Socket& client, const PlugFSHeader& header)
	{
		std::uint64_t total_xfer_size = std::accumulate(
			header._filemeta.begin(),
			header._filemeta.end(),
			0,
			[&](const auto& sum, const auto& v) -> std::uint64_t {
				return sum + v._size;
		});

		LogDebug("[mxplugfs] Expecting xfer with content size: %llu bytes.", total_xfer_size);

		auto files_buffer = PlugFSRecvTimeout(client, total_xfer_size);
		if(!files_buffer.has_value())
		{
			return std::nullopt;
		}

		auto files_buffer_data = files_buffer.value();
		const std::uint8_t* ptr = files_buffer_data.data();
		std::vector<PlugFSFileData> files;
		for(const auto& meta : header._filemeta)
		{
			files.push_back({ ptr, ptr + meta._size });
			ptr += meta._size;
		}

		return files;
	}

	static void PlugFSWriteFileWithMetadata(const PlugFSFileData& file, const PlugFSFileMeta& meta, const std::string& dir)
	{
		// We write at the given experiment location
		const std::string file_path = dir + "/" + meta._filename.c_str();
		LogDebug("[mxplugfs] Writting file <%s>.", file_path.c_str());
		SysWriteBinFile(file_path, file);

		// NOTE: (César) clock_cast is not yet available under GCC for C++20
		auto from_systime = [](const std::int64_t& time) -> std::filesystem::file_time_type {
			std::int64_t delta = mulex::SysGetCurrentTime() - time;
			return std::filesystem::file_time_type{std::filesystem::file_time_type::clock::now() - std::chrono::milliseconds(delta)};
		};

		// Set file basic metadata
		std::filesystem::last_write_time(
			file_path.c_str(),
			from_systime(meta._mod_time)
		);
	}

	static PlugFSFileMeta PlugFSMakeDirs(const PlugFSFileMeta& meta, const std::string& dir)
	{
		std::string filename = meta._filename.c_str();

		// In case the file came from Windows we might have backslashes instead
		// Windows also recognizes paths with forward slashes so change it
		// (at least from the Win32 API that I know)
		std::replace(filename.begin(), filename.end(), '\\', '/');

		auto pos = filename.find_last_of('/');
		if(pos == std::string::npos)
		{
			return meta;
		}

		// Create nested subdirs if they don't exist
		std::filesystem::create_directories(dir + "/" + filename.substr(0, pos));

		// Update original metadata filename to write
		return PlugFSFileMeta {
			._filename = filename,
			._size = meta._size,
			._mod_time = meta._mod_time
		};
	}

	static void PlugFSWriteFiles(const std::vector<PlugFSFileData>& files, const std::vector<PlugFSFileMeta>& meta, const std::string& expname)
	{
		const std::string plug_dir = PlugFSGetExperimentPluginsDir(expname);

		// Ranges views zip is only C++23 =[
		for(std::uint64_t i = 0; i < files.size(); i++)
		{
			PlugFSFileMeta nmeta = PlugFSMakeDirs(meta[i], plug_dir);
			PlugFSWriteFileWithMetadata(files[i], nmeta, plug_dir);
		}
	}

	void PlugFSLoop()
	{
		bool would_block;
		Socket client = SocketAccept(_server_socket, &would_block);
		if(would_block)
		{
			return;
		}

		if(!client._error)
		{
			auto exp_name = PlugFSReceiveExperimentName(client);

			if(!exp_name.has_value() || !PlugFSCheckExperimentExists(exp_name.value()))
			{
				PlugFSSendData(client, { 0 }); // Send 0 ACK -> NOT OK
				SocketClose(client);
				LogError("[mxplugfs] Experiment refered name does not exist on cache.");
				return;
			}

			PlugFSSendData(client, { 1 }); // Send 1 ACK -> OK

			auto header = PlugFSReceiveHeader(client);
			if(header.has_value())
			{
				auto files = PlugFSReceiveFiles(client, header.value());
				if(files.has_value())
				{
					LogTrace("[mxplugfs] Writting files from remote connection...");
					PlugFSWriteFiles(files.value(), header.value()._filemeta, exp_name.value());
				}
			}

			PlugFSSendData(client, { 1 }); // Send 1 ACK -> OK

			SocketClose(client);
		}
	}

	void PlugFSClose()
	{
		SocketClose(_server_socket);
	}

	void PlugFSInitServerThread()
	{
		_server_run.store(true);
		_server_thread = std::make_unique<std::thread>([]() {

			LogDebug("[mxplugfs] Initializing file server...");

			if(!PlugFSInitialize())
			{
				LogError("[mxplugfs] Failed to initialize plugfs server. Remote plugin copy will not be available.");
				_server_run.store(false);
				return;
			}
			
			while(_server_run.load())
			{
				PlugFSLoop();
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}

			LogDebug("[mxplugfs] Closing plugfs server...");
			PlugFSClose();
		});
	}

	void PlugFSShutdownServerThread()
	{
		_server_run.store(false);
		if(_server_thread->joinable())
		{
			_server_thread->join();
		}
	}

	Socket PlugFSConnectToServer(const std::string& host)
	{
		Socket socket = SocketInit();
		SocketConnect(socket, host, FXFER_PORT, FXFER_TIMEOUT); // NOTE: (Cesar) Timeout after 5 seconds

		if(socket._error)
		{
			LogError("[mxplugfs] Failed to connect to plugfs server at: %s:%d", host.c_str(), FXFER_PORT);
		}

		return socket;
	}

	bool PlugFSCheckExperimentRemote(const Socket& socket, const std::string& experiment)
	{
		if(socket._error)
		{
			return false;
		}

		mxstring<512> exp = experiment;
		std::vector<std::uint8_t> buffer(512);
		std::memcpy(buffer.data(), &exp, 512);
		PlugFSSendData(socket, buffer);

		// Receive ACK byte
		auto ack = PlugFSRecvTimeout(socket, 1);
		std::uint8_t ok = ack.value_or(std::vector<std::uint8_t>{ 0 })[0];
		return ok != 0;
	}

	bool PlugFSTransfer(const Socket& socket, const std::string& dir)
	{
		auto [metadata, contents] = PlugFSCreateMetadataAndData(dir);

		std::uint64_t xfer_size = metadata.size() * sizeof(PlugFSFileMeta);
		std::vector<std::uint8_t> buffer(xfer_size + sizeof(std::uint64_t));

		std::memcpy(buffer.data(), &xfer_size, sizeof(std::uint64_t));
		std::memcpy(buffer.data() + sizeof(std::uint64_t), metadata.data(), xfer_size);

		// Metadata
		PlugFSSendData(socket, buffer);

		std::uint64_t total_data = std::accumulate(contents.begin(), contents.end(), 0, [](std::uint64_t sum, const auto& v) -> std::uint64_t {
			return sum + v.size();
		});

		buffer.clear();
		buffer.reserve(total_data);
		for(const auto& content : contents)
		{
			buffer.insert(buffer.end(), content.begin(), content.end());
		}

		// Contents
		PlugFSSendData(socket, buffer);

		auto ack = PlugFSRecvTimeout(socket, 1);
		std::uint8_t ok = ack.value_or(std::vector<std::uint8_t>{ 0 })[0];
		return ok != 0;
	}

	void PlugFSDisconnectFromServer(Socket& socket)
	{
		if(!socket._error)
		{
			SocketClose(socket);
		}
	}
}
