#include <string>
#include <cstdint>

namespace mulex
{
	struct DrvSerial
	{
#ifdef __linux__
		int _handle;
#else
		HANDLE _handle;
#endif
		bool _error;
	};

	enum class DrvSerialResult
	{
		ERROR,
		READ_OK,
		WRITE_OK,
		READ_PARTIAL,
		WRITE_PARTIAL
	};

	struct DrvSerialArgs
	{
		int baud;
		int parity;
		int flags;
		bool blocking;
	};

	DrvSerial DrvSerialInit(const std::string& portname, const DrvSerialArgs& args);
	DrvSerialResult DrvSerialWrite(const DrvSerial& serial, const std::uint8_t* buffer, std::uint64_t len);
	DrvSerialResult DrvSerialRead(const DrvSerial& serial, std::uint8_t* buffer, std::uint64_t len, std::uint64_t* rlen);
	void DrvSerialClose(const DrvSerial& serial);
} // namespace mulex
