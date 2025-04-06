#include <string>
#include <cstdint>

#include "network/socket.h"

#ifdef _WIN32
#include <windows.h>
#undef ERROR
#endif

#ifdef USB_SUPPORT
#ifdef __linux__
#include <libusb-1.0/libusb.h>
#else
#include <libusb.h>
// #include <libusb-1.0/libusb.h>
#endif
#endif

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
#ifdef __linux__
		int flags;
#else
		long unsigned int flags;
#endif
		bool blocking;
	};

	DrvSerial DrvSerialInit(const std::string& portname, const DrvSerialArgs& args);
	DrvSerialResult DrvSerialWrite(const DrvSerial& serial, const std::uint8_t* buffer, std::uint64_t len);
	DrvSerialResult DrvSerialRead(const DrvSerial& serial, std::uint8_t* buffer, std::uint64_t len, std::uint64_t* rlen);
	void DrvSerialClose(const DrvSerial& serial);

	struct DrvTCP
	{
		Socket _socket;
		std::uint16_t _timeout;
	};

	enum class DrvTCPResult
	{
		ERROR,
		RECV_OK,
		RECV_TIMEOUT,
		SEND_OK
	};

	DrvTCP DrvTCPInit(const std::string& hostname, const std::uint16_t& port, const std::uint16_t& recvtimeout = 0);
	DrvTCPResult DrvTCPSend(const DrvTCP& handle, std::uint8_t* buffer, std::uint64_t len);
	DrvTCPResult DrvTCPRecv(const DrvTCP& handle, std::uint8_t* buffer, std::uint64_t len, std::uint64_t exlen, std::uint64_t* rlen);
	void DrvTCPClose(DrvTCP& handle);

#ifdef USB_SUPPORT
	struct DrvUSB
	{
		libusb_device_handle* _handle;
		bool _error;
		std::uint8_t _endpoint_inbulk;
		std::uint8_t _endpoint_outbulk;
	};

	enum class DrvUSBResult
	{
		ERROR,
		WRITE_OK,
		READ_OK
	};

	DrvUSB DrvUSBInit(const std::string& devpidvid);
	DrvUSBResult DrvUSBWriteBulk(const DrvUSB& handle, std::uint8_t* buffer, std::uint64_t len, std::uint64_t* wlen);
	DrvUSBResult DrvUSBReadBulk(const DrvUSB& handle, std::uint8_t* buffer, std::uint64_t len, std::uint64_t* rlen);
	void DrvUSBClose(DrvUSB& handle);
#endif
} // namespace mulex
