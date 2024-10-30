#include "test.h"
#include "../mxdrv.h"
#include "../mxlogger.h"
#include <termios.h>
#include <pty.h>
#include <fcntl.h>
#include <cstring>
#include <thread>
#include <unistd.h>
#include <mutex>

int main(int argc, char* argv[])
{
	using namespace mulex;
	std::mutex m;
	char name[256];

	// Setup a pty for testing
	std::thread([&m, &name](){
		int master, slave;

		struct termios tty;
		tty.c_iflag = (tcflag_t) 0;
		tty.c_lflag = (tcflag_t) 0;
		tty.c_cflag = CS8;
		tty.c_oflag = (tcflag_t) 0;

		m.lock();
		int err = ::openpty(&master, &slave, name, &tty, nullptr);
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
	
	DrvSerialArgs sa = { B115200, 0, O_RDWR | O_NOCTTY | O_SYNC, false };
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
	return 0;
}
