#include "../mxdrv.h"

#ifdef __linux__
#include <termios.h>
#include <fcntl.h>
#endif
#include <string.h>
#include <unistd.h>

#include "../mxlogger.h"

namespace mulex
{
#ifdef __linux__
	static speed_t BaudRateFromInt(int baud)
	{
		switch (baud) {
			case 9600:
				return B9600;
			case 19200:
				return B19200;
			case 38400:
				return B38400;
			case 57600:
				return B57600;
			case 115200:
				return B115200;
			case 230400:
				return B230400;
			case 460800:
				return B460800;
			case 500000:
				return B500000;
			case 576000:
				return B576000;
			case 921600:
				return B921600;
			case 1000000:
				return B1000000;
			case 1152000:
				return B1152000;
			case 1500000:
				return B1500000;
			case 2000000:
				return B2000000;
			case 2500000:
				return B2500000;
			case 3000000:
				return B3000000;
			case 3500000:
				return B3500000;
			case 4000000:
				return B4000000;
			default: 
				LogError("Could not convert to non standard baud rate: %d", baud);
				LogError("See 'man cfsetospeed' for a list of supported baud rates");
				return B0;
		}
	}
#endif

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

		speed_t baud = BaudRateFromInt(args.baud);
		if(baud == B0)
		{
			output._error = true;
			LogError("Failed to assing baud rate under %s", portname.c_str());
			return output;
		}

		cfsetospeed(&tty, baud);
		cfsetispeed(&tty, baud);
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
		output._handle = CreateFile(portname.c_str(), args.flags, 0, NULL, OPEN_EXISTING, 0, NULL);

		if(output._handle == INVALID_HANDLE_VALUE)
		{
			output._error = true;
			LogError("CreateFile failed to open %s with error %u", portname.c_str(), GetLastError());
			return output;
		}

		DCB tty = { 0 };
		tty.DCBlength = sizeof(tty);
		BOOL status = GetCommState(output._handle, &tty);

		if(status == FALSE)
		{
			LogDebug("Failed to automatically detect params for DCB, using hint values");
		}

		if(tty.BaudRate != args.baud)
		{
			LogWarning("Specified baud %u does not match port baud of %u", args.baud, tty.BaudRate);
			LogWarning("Using specified value");
		}

		tty.BaudRate = args.baud;
		tty.ByteSize = tty.ByteSize ? tty.ByteSize : 8;
		tty.StopBits = tty.StopBits ? tty.StopBits : ONESTOPBIT;
		tty.Parity   = tty.Parity   ? tty.Parity   : NOPARITY;

		status = SetCommState(output._handle, &tty);

		if(status == FALSE)
		{
			LogWarning("DCB parameter setting for serial failed. Using default values if applicable");
		}

		// Use some default timeouts
		COMMTIMEOUTS timeouts = { 0 };
		timeouts.ReadIntervalTimeout = 50;
		timeouts.ReadTotalTimeoutConstant = 50;
		timeouts.ReadTotalTimeoutMultiplier = 10;
		timeouts.WriteTotalTimeoutConstant = 50;
		timeouts.WriteTotalTimeoutMultiplier = 10;

		status = SetCommTimeouts(output._handle, &timeouts);
		if(status == FALSE)
		{
			LogWarning("Failed to set serial timeouts. Using none");
		}
#endif
		LogDebug("Openning serial handle %d [%s]", output._handle, portname.c_str());
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
			LogTrace("DrvSerialWrite wrote %d bytes.", nw);
			return DrvSerialResult::WRITE_PARTIAL;
		}
		LogTrace("DrvSerialWrite wrote %d bytes.", nw);
		return DrvSerialResult::WRITE_OK;
#else
		DWORD nw = 0;
		BOOL status = ::WriteFile(serial._handle, reinterpret_cast<const char*>(buffer), len, &nw, NULL);
		if(status == FALSE)
		{
			LogError("Failed to write bytes to serial handle: %d", serial._handle);
			return DrvSerialResult::ERROR;
		}
		else if(nw < len)
		{
			LogTrace("DrvSerialWrite wrote %d bytes.", nw);
			return DrvSerialResult::WRITE_PARTIAL;
		}
		LogTrace("DrvSerialWrite wrote %d bytes.", nw);
		return DrvSerialResult::WRITE_OK;
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
		DWORD emask = 0;
		BOOL status = ::SetCommMask(serial._handle, EV_RXCHAR);
		if(status == FALSE)
		{
			LogError("Failed to set COM mask. Error %u", GetLastError());
			return DrvSerialResult::ERROR;
		}

		status = ::WaitCommEvent(serial._handle, &emask, NULL);
		if(status == FALSE)
		{
			LogError("Failed to wait COM event. Error %u", GetLastError());
			return DrvSerialResult::ERROR;
		}

		int i = 0;
		if(rlen) *rlen = 0;
		while(true)
		{
			std::int8_t byte;
			DWORD nw;
			status = ::ReadFile(serial._handle, &byte, 1, &nw, NULL);
			if(status == FALSE) break;
			if(byte == '\n' || nw == 0) break;

			if(i >= len)
			{
				// NOTE: (Cesar) This does not read the rest of the buffer
				// 				 Even though we want to avoid this error
				//				 perhaps one should guard against it on the runtime
				LogError("Serial read buffer too small. Truncating output");
				return DrvSerialResult::READ_PARTIAL;
			}

			buffer[i++] = byte;
		}
		if(rlen) *rlen = i;
		return DrvSerialResult::READ_OK;
#endif
	}

	void DrvSerialClose(const DrvSerial& serial)
	{
		if(!serial._error)
		{
			LogDebug("Closing serial handle %d.", serial._handle);
#ifdef __linux__
			::close(serial._handle);
#else
			::CloseHandle(serial._handle);
#endif
		}
	}
} // namespace mulex
