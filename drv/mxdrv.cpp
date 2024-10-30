#include "../mxdrv.h"
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "../mxlogger.h"

namespace mulex
{
	DrvSerial DrvSerialInit(const std::string& portname, const DrvSerialArgs& args)
	{
		DrvSerial output;
		output._error = false;
#ifdef __unix__
		output._handle = ::open(portname.c_str(), static_cast<int>(args.flags));
		if(output._handle < 0)
		{
			output._error = true;
			LogError("Failed to open serial under %s: %s", portname.c_str(), strerror(errno));
			return output;
		}

		// Now set serial attributes such as baud rate, n-bit chars, etc...
		struct termios tty;
		if(tcgetattr(output._handle, &tty) != 0)
		{
			output._error = true;
			LogError("tcgetattr failed: %d", errno);
			return output;
		}

		cfsetospeed(&tty, args.baud);
		cfsetispeed(&tty, args.baud);
		tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
		tty.c_iflag &= ~IGNBRK;
		tty.c_lflag = 0;
		tty.c_oflag = 0;
		tty.c_cc[VMIN] = args.blocking ? 1 : 0; // Blocking read ?
		tty.c_cc[VTIME] = 5; // 500ms read timeout
		
		tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Disable xon/xoff
		tty.c_cflag |= (CLOCAL | CREAD);
		tty.c_cflag &= ~(PARENB | PARODD);
		tty.c_cflag |= args.parity;
		tty.c_cflag &= ~CSTOP;
		tty.c_cflag &= ~CRTSCTS;

		if(tcsetattr(output._handle, TCSANOW, &tty) != 0)
		{
			output._error = true;
			LogError("tcsetattr failed: %d", errno);
			return output;
		}
#else
#endif
		LogDebug("Openning serial handle %d.", output._handle);
		return output;
	}

	DrvSerialResult DrvSerialWrite(const DrvSerial& serial, const std::uint8_t* buffer, std::uint64_t len)
	{
#ifdef __linux__
		std::int64_t nw = ::write(serial._handle, buffer, len);
		if(nw < 0)
		{
			LogError("Failed to write bytes to serial handle: %d", serial._handle);
			return DrvSerialResult::ERROR;
		}
		else if(nw < len)
		{
			return DrvSerialResult::WRITE_PARTIAL;
		}
		LogTrace("DrvSerialWrite wrote %d bytes.", nw);
		return DrvSerialResult::WRITE_OK;
#else
#endif
	}

	DrvSerialResult DrvSerialRead(const DrvSerial& serial, std::uint8_t* buffer, std::uint64_t len, std::uint64_t* rlen)
	{
#ifdef __linux__
		if(rlen) *rlen = 0;
		std::int64_t nw = ::read(serial._handle, buffer, len);
		if(nw < 0)
		{
			LogError("Failed to read bytes from serial handle: %d", serial._handle);
			LogError("Error: %s", strerror(errno));
			return DrvSerialResult::ERROR;
		}

		if(rlen) *rlen = static_cast<std::uint64_t>(nw);
		LogTrace("DrvSerialRead read %d bytes.", nw);
		return DrvSerialResult::READ_OK;
#else
#endif
	}

	void DrvSerialClose(const DrvSerial& serial)
	{
		if(!serial._error)
		{
			LogDebug("Closing serial handle %d.", serial._handle);
			::close(serial._handle);
		}
	}
} // namespace mulex
