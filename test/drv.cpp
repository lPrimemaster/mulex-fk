#include "test.h"
#include "../mxdrv.h"
#include "../mxlogger.h"

#ifdef __linux__
#include <termios.h>
#include <pty.h>
#include <fcntl.h>
#else
#include <Windows.h>
#endif
#include <cstring>
#include <thread>
#include <unistd.h>
#include <mutex>

void test_serial()
{
	using namespace mulex;
	std::mutex m;
	char name[256];

	LogDebug("test_serial()");

#ifdef __linux__
	// Setup a pty for testing
	std::thread([&m, &name](){
		int master, slave;

		struct termios tty;
		tty.c_iflag = (tcflag_t) 0;
		tty.c_lflag = (tcflag_t) 0;
		tty.c_cflag = CS8;
		tty.c_oflag = (tcflag_t) 0;

		m.lock();
		::openpty(&master, &slave, name, &tty, nullptr);
		m.unlock();

		int r = ::read(master, name, sizeof(name)-1);
		name[r] = 0;
		std::cout << "THR: " << name << std::endl;
		::write(master, "Hello Back\0", 11);

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		::close(slave);
		::close(master);
	}).detach();

	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	
	DrvSerialArgs sa = { 115200, 0, O_RDWR | O_NOCTTY | O_SYNC, true };
#else
	// TODO: (Cesar) Setup a ConPTY for testing
	// 				 For now using a COM bridge on Windows
	std::thread([](){
		DrvSerialArgs sa = { 9600, 0, GENERIC_READ | GENERIC_WRITE, true };

		DrvSerial serial = DrvSerialInit("COM2", sa);
		std::uint8_t buffer[64];
		std::uint64_t rlen;
		DrvSerialRead(serial, buffer, 64, &rlen);

		std::cout << "THR: " << reinterpret_cast<char*>(buffer) << std::endl;

		std::memcpy(buffer, "Hello Back\0", 11);
		DrvSerialWrite(serial, buffer, 11);

		DrvSerialClose(serial);
	}).detach();

	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	
	DrvSerialArgs sa = { 9600, 0, GENERIC_READ | GENERIC_WRITE, true };
	strcpy(name, "COM3");
#endif

	m.lock();
	LogDebug("Testing using %s", name);
	DrvSerial serial = DrvSerialInit(name, sa);
	m.unlock();
	std::uint8_t buffer[64];
	std::memcpy(buffer, "Hello\n", 6);
	DrvSerialWrite(serial, buffer, 6);

	std::uint64_t rlen;
	DrvSerialRead(serial, buffer, 64, &rlen);

	std::cout << "MAIN: " << reinterpret_cast<char*>(buffer) << std::endl;
	ASSERT_THROW(std::string(reinterpret_cast<char*>(buffer)) == "Hello Back");

	DrvSerialClose(serial);
}

void test_tcp()
{
	using namespace mulex;
	LogDebug("test_tcp()");

	// Setup echo server
	std::thread thr([](){
		Socket ss = SocketInit();
		Socket client;
		SocketBindListen(ss, 42069);

		SocketSetNonBlocking(ss);
		bool wb = true;
		while(wb)
		{
			client = SocketAccept(ss, &wb);
		}

		std::uint8_t rbuffer[256];
		std::uint64_t rlen = 0;
		std::uint64_t tlen = 0;

		while(tlen < 8)
		{
			if(SocketRecvBytes(client, rbuffer + tlen, 256 - tlen, &rlen) == SocketResult::ERROR)
			{
				break;
			}
			tlen += rlen;
		}

		std::uint64_t msgsz = *reinterpret_cast<std::uint64_t*>(rbuffer);

		LogTrace("THR: Len %llu", msgsz);
		LogTrace("THR: tlen %llu", tlen);
		
		while(tlen < 8 + msgsz)
		{
			if(SocketRecvBytes(client, rbuffer + tlen, 256 - tlen, &rlen) == SocketResult::ERROR)
			{
				break;
			}
			tlen += rlen;
		}

		char* str = reinterpret_cast<char*>(rbuffer + 8);
		LogTrace("THR: Got %s", str);
		LogTrace("THR: tlen %llu", tlen);

		SocketSendBytes(client, rbuffer + 8, tlen - 8);

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		SocketClose(client);
		SocketClose(ss);
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	DrvTCP handle = DrvTCPInit("localhost", 42069, 500); // 500 ms timeout
	
	std::uint8_t buffer[256];
	std::uint8_t bufferin[256];
	std::uint64_t rb;

	std::string msg = "Noah's ark down the river it goes.";
	std::uint64_t mlen = msg.length() + 1;
	std::memcpy(buffer, &mlen, sizeof(std::uint64_t));
	DrvTCPSend(handle, buffer, sizeof(std::uint64_t));
	std::memcpy(buffer, msg.c_str(), mlen);
	DrvTCPSend(handle, buffer, mlen);
	DrvTCPRecv(handle, bufferin, 256, mlen, &rb);

	LogDebug("\n\tsent: %s\n\treturned: %s", msg.c_str(), reinterpret_cast<char*>(bufferin));
	
	DrvTCPClose(handle);

	thr.join();

	ASSERT_THROW(msg == reinterpret_cast<char*>(bufferin));
}

void test_usb()
{
	using namespace mulex;
	LogDebug("test_usb()");
#ifdef USB_SUPPORT
	DrvUSB handle = DrvUSBInit("0DB0-0D08");
	// TODO: (Cesar) Implement some sort of usb emulation
	DrvUSBClose(handle);
#endif
}

int main(int argc, char* argv[])
{
	test_serial();
	test_tcp();
	test_usb();
	return 0;
}
