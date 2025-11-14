#include "../mxdrv.h"

#ifdef __linux__
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include <string.h>
#include <thread>

#include "../mxlogger.h"
#include "../mxsystem.h"

namespace mulex
{
#ifdef __linux__
	static speed_t BaudRateFromInt(int baud)
	{
		switch (baud) {
			case 50:
				return B50;
			case 75:
				return B75;
			case 110:
				return B110;
			case 134:
				return B134;
			case 150:
				return B150;
			case 200:
				return B200;
			case 300:
				return B300;
			case 600:
				return B600;
			case 1200:
				return B1200;
			case 1800:
				return B1800;
			case 2400:
				return B2400;
			case 4800:
				return B4800;
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
		tty.c_cflag &= ~CSTOPB;
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

		if(tty.BaudRate != static_cast<DWORD>(args.baud))
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
		else if(static_cast<std::uint64_t>(nw) < len)
		{
			LogTrace("DrvSerialWrite wrote %d bytes.", nw);
			return DrvSerialResult::WRITE_PARTIAL;
		}
		LogTrace("DrvSerialWrite wrote %d bytes.", nw);
		return DrvSerialResult::WRITE_OK;
#else
		DWORD nw = 0;
		BOOL status = ::WriteFile(serial._handle, reinterpret_cast<const char*>(buffer), static_cast<DWORD>(len), &nw, NULL);
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
				if(rlen) *rlen = len;
				LogTrace("DrvSerialRead read %d bytes.", len);
				return DrvSerialResult::READ_PARTIAL;
			}

			buffer[i++] = byte;
		}
		if(rlen) *rlen = i;
		LogTrace("DrvSerialRead read %d bytes.", i);
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

	DrvTCP DrvTCPInit(const std::string& hostname, const std::uint16_t& port, const std::uint16_t& recvtimeout)
	{
		DrvTCP handle;
		handle._timeout = recvtimeout;
		handle._socket = SocketInit();
		if(handle._socket._error)
		{
			return handle;
		}

		SocketConnect(handle._socket, hostname, port);

		if(handle._socket._error)
		{
			LogError("DrvTCP failed to connect to socket at: %s:%d", hostname.c_str(), port);
		}
		else
		{
			LogDebug("DrvTCP connected to: %s:%d", hostname.c_str(), port);
		}

		if(recvtimeout == 0)
		{
			LogWarning("DrvTCP running without recv timeout. This is unrecommended");
			LogWarning("DrvTCPRecv is a blocking operation");
		}
		
		return handle;
	}

	DrvTCPResult DrvTCPSend(const DrvTCP& handle, std::uint8_t* buffer, std::uint64_t len)
	{
		if(SocketSendBytes(handle._socket, buffer, len) == SocketResult::ERROR)
		{
			return DrvTCPResult::ERROR;
		}
		return DrvTCPResult::SEND_OK;
	}

	DrvTCPResult DrvTCPRecv(const DrvTCP& handle, std::uint8_t* buffer, std::uint64_t len, std::uint64_t exlen, std::uint64_t* rlen)
	{
		if(!rlen)
		{
			LogError("DrvTCP rlen pointer cannot be nullptr");
			return DrvTCPResult::ERROR;
		}

		if(exlen > len)
		{
			LogError("DrvTCP buffer as apparent len of %llu but expects %llu bytes", len, exlen);
			return DrvTCPResult::ERROR;
		}

		std::int64_t ms = SysGetCurrentTime();
		std::uint64_t totlen = 0;

		while(exlen > 0)
		{
			if(handle._timeout > 0 && (SysGetCurrentTime() - ms >= handle._timeout))
			{
				*rlen = totlen;
				LogTrace("DrvTCP received %llu bytes, expected %llu", totlen, exlen);
				LogWarning("DrvTCP timeout reading");
				return DrvTCPResult::RECV_TIMEOUT;
			}

			std::this_thread::sleep_for(std::chrono::microseconds(10));
			SocketResult result = SocketRecvBytes(handle._socket, buffer + totlen, len - totlen, rlen);
			
			if(result == SocketResult::ERROR || result == SocketResult::DISCONNECT)
			{
				LogError("DrvTCP failed to receive data");
				return DrvTCPResult::ERROR;
			}
			
			exlen -= *rlen;
			totlen += *rlen;
		}

		*rlen = totlen;
		LogTrace("DrvTCP received %llu bytes", totlen);
		return DrvTCPResult::RECV_OK;
	}

	void DrvTCPClose(DrvTCP& handle)
	{
		if(!handle._socket._error)
		{
			SocketClose(handle._socket);
		}
		LogDebug("DrvTCP disconnected");
	}

#ifdef USB_SUPPORT
	class LibUsbGuard
	{
	public:
		LibUsbGuard()
		{
			if(libusb_init(&_ctx) < 0)
			{
				_error = true;
				LogError("LibUsbGuard failed to init libusb context");
			}
			else
			{
				LogTrace("LibUsb initialized");
			}
		}

		libusb_context* operator&()
		{
			return _ctx;
		}

		~LibUsbGuard()
		{
			if(_ctx)
			{
				libusb_exit(_ctx);
				LogTrace("LibUsb terminated");
			}
		}

		bool ok() const
		{
			return !_error;
		}

	private:
		libusb_context* _ctx = nullptr;
		bool _error = false;
	};
	
	DrvUSB DrvUSBInit(const std::string& devpidvid)
	{
		static LibUsbGuard _libusbguard;
		DrvUSB handle;
		handle._error = false;
		
		if(!_libusbguard.ok())
		{
			handle._error = true;
			LogError("LibUsb context is not valid");
			return handle;
		}

		std::uint16_t vid = std::stoi(devpidvid.substr(0, 4), 0, 16);
		std::uint16_t pid = std::stoi(devpidvid.substr(5, 4), 0, 16);
		LogTrace("Translated vid - %d", vid);
		LogTrace("Translated pid - %d", pid);
		handle._handle = libusb_open_device_with_vid_pid(&_libusbguard, vid, pid);

		if(!handle._handle)
		{
			handle._error = true;
			LogError("DrvUSB could not find device: %s", devpidvid.c_str());
			return handle;
		}

		libusb_config_descriptor* config;
		if(libusb_get_active_config_descriptor(libusb_get_device(handle._handle), &config) < 0)
		{
			handle._error = true;
			LogError("DrvUSB failed to find config descriptor: %s", devpidvid.c_str());
			libusb_close(handle._handle);
			return handle;
		}

		if(config->bNumInterfaces < 1)
		{
			handle._error = true;
			LogError("DrvUSB device %s does not contain interfaces", devpidvid.c_str());
			libusb_free_config_descriptor(config);
			libusb_close(handle._handle);
			return handle;
		}

		handle._endpoint_inbulk = 0;
		handle._endpoint_outbulk = 0;

		libusb_interface interface = config->interface[0];
		for(int i = 0; i < interface.num_altsetting; i++)
		{
	  		for(int e = 0; e < interface.altsetting[i].bNumEndpoints; e++)
			{
				libusb_endpoint_descriptor endpoint = interface.altsetting[i].endpoint[e];
				if((endpoint.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN)
				{
					switch(endpoint.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK)
		 			{
						case LIBUSB_TRANSFER_TYPE_BULK:
							handle._endpoint_inbulk = endpoint.bEndpointAddress;
							LogTrace("Found bulk IN endpoint: %d", endpoint.bEndpointAddress);
							continue;
						default:
							LogTrace("Found non-bulk IN endpoint: %d", endpoint.bEndpointAddress);
							break; // Skip non-bulk enpoints (not supported for now)
					}
				}

				if((endpoint.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT)
				{
					switch(endpoint.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK)
		 			{
						case LIBUSB_TRANSFER_TYPE_BULK:
							handle._endpoint_outbulk = endpoint.bEndpointAddress;
							LogTrace("Found bulk OUT endpoint: %d", endpoint.bEndpointAddress);
							continue;
						default:
							LogTrace("Found non-bulk OUT endpoint: %d", endpoint.bEndpointAddress);
							break; // Skip non-bulk enpoints (not supported for now)
					}
				}
			}
		}
		
		if((handle._endpoint_inbulk == 0) || (handle._endpoint_outbulk == 0))
		{
			handle._error = true;
			LogError("Failed to find bulk endpoints for device: %s", devpidvid.c_str());
			libusb_free_config_descriptor(config);
			libusb_close(handle._handle);
			return handle;
		}

		if(libusb_claim_interface(handle._handle, 0) < 0)
		{
			handle._error = true;
			LogError("DrvUSB could not claim device interface 0: %s", devpidvid.c_str());
			libusb_free_config_descriptor(config);
			libusb_close(handle._handle);
			return handle;
		}
		
		libusb_free_config_descriptor(config);
		LogDebug("DrvUSB connected to: %s", devpidvid.c_str());
		return handle;
	}

	DrvUSBResult DrvUSBWriteBulk(const DrvUSB& handle, std::uint8_t* buffer, std::uint64_t len, std::uint64_t* wlen)
	{
		int rb;
		int res = libusb_bulk_transfer(handle._handle, handle._endpoint_outbulk, buffer, static_cast<int>(len), &rb, 0);
		
		if(res == 0)
		{
			if(wlen) *wlen = rb;
			LogTrace("DrvUSBWriteBulk() wrote %d bytes", rb);
			return DrvUSBResult::WRITE_OK;
		}

		if(wlen) *wlen = 0;
		LogError("DrvUSBWriteBulk() failed: %s", libusb_error_name(res));
		return DrvUSBResult::ERROR;
	}

	DrvUSBResult DrvUSBReadBulk(const DrvUSB& handle, std::uint8_t* buffer, std::uint64_t len, std::uint64_t* rlen)
	{
		int rb;
		int res = libusb_bulk_transfer(handle._handle, handle._endpoint_inbulk, buffer, static_cast<int>(len), &rb, 0);
		
		if(res == 0)
		{
			if(rlen) *rlen = rb;
			LogTrace("DrvUSBReadBulk() read %d bytes", rb);
			return DrvUSBResult::READ_OK;
		}

		if(rlen) *rlen = 0;
		LogError("DrvUSBReadBulk() failed: %s", libusb_error_name(res));
		return DrvUSBResult::ERROR;
	}

	void DrvUSBClose(DrvUSB& handle)
	{
		if(!handle._error)
		{
			if(libusb_release_interface(handle._handle, 0) < 0)
			{
				LogError("DrvUSB failed to release interface 0");
			}
			libusb_close(handle._handle);
		}
		LogDebug("DrvUSB closing handle");
	}
#endif

} // namespace mulex
