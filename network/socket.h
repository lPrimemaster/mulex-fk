#pragma once
#include <cstdint>
#include <atomic>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#undef ERROR
#endif

namespace mulex
{

#ifdef __unix__
	struct Socket
	{
		int _handle;
		bool _error;
		char _addr[32];
	};
#else
	struct Socket
	{
		SOCKET _handle;
		bool _error;
		char _addr[32];
	};
#endif

	enum class SocketResult
	{
		OK,
		DISCONNECT,
		ERROR,
		TIMEOUT
	};

	bool operator<(const Socket& lhs, const Socket& rhs);

	Socket SocketInit();
	void SocketBindListen(Socket& socket, std::uint16_t port);
	bool SocketSetNonBlocking(const Socket& socket);
	bool SocketSetBlocking(const Socket& socket);
	bool SocketAwaitConnection(Socket& socket, std::int64_t timeout);
	Socket SocketAccept(const Socket& socket, bool* would_block);
	SocketResult SocketRecvBytes(const Socket& socket, std::uint8_t* buffer, std::uint64_t len, std::uint64_t* rlen);
	SocketResult SocketSendBytes(const Socket& socket, std::uint8_t* buffer, std::uint64_t len);
	void SocketConnect(Socket& socket, const std::string& hostname, std::uint16_t port, std::int64_t timeout = 0);
	void SocketClose(Socket& socket);

} // namespace mulex
