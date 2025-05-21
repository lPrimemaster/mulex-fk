#include <cstdint>
#include "../mxtypes.h"
#include "../network/socket.h"
#include <vector>

namespace mulex
{
	static constexpr std::uint16_t FXFER_PORT = 5704;
	static constexpr std::int64_t FXFER_TIMEOUT = 5000;
	using PlugFSFilename = mulex::mxstring<1024>;
	using PlugFSFileData = std::vector<std::uint8_t>;

	struct PlugFSFileMeta
	{
		PlugFSFilename _filename; // Filename relative
		std::uint64_t  _size;
		std::int64_t   _mod_time;
	};

	struct PlugFSHeader
	{
		std::uint64_t 				_xfer_size; // Excludes header size
		std::vector<PlugFSFileMeta> _filemeta;
	};

	bool PlugFSInitialize();
	void PlugFSLoop();
	void PlugFSClose();
	
	void PlugFSInitServerThread();
	void PlugFSShutdownServerThread();

	Socket PlugFSConnectToServer(const std::string& host);
	bool PlugFSCheckExperimentRemote(const Socket& socket, const std::string& experiment);
	bool PlugFSTransfer(const Socket& socket, const std::string& dir);
	void PlugFSDisconnectFromServer(Socket& socket);
}
