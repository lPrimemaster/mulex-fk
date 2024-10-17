#pragma once
#include <cstdint>
#include <atomic>
#include <string>

namespace mulex
{

#ifdef __unix__
	struct Socket
	{
		int _handle;
		bool _error;
	};
#else
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
	Socket SocketAccept(const Socket& socket, bool* would_block);
	SocketResult SocketRecvBytes(const Socket& socket, std::uint8_t* buffer, std::uint64_t len, std::atomic<bool>* notify_unblock, std::uint32_t timeout_ms = 0);
	SocketResult SocketSendBytes(const Socket& socket, std::uint8_t* buffer, std::uint64_t len);
	void SocketConnect(Socket& socket, const std::string& hostname, std::uint16_t port);
	void SocketClose(Socket& socket);

} // namespace mulex
