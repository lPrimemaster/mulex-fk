// Author : César Godinho
// Date   : 10/10/2024
// Brief  : Common functions for setting up sockets on Windows and Linux

#include "socket.h"

#ifdef __unix__
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#else
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <ws2tcpip.h>
#include <WinSock2.h>
#endif

#include "../mxlogger.h"

#include <cstring>

#ifdef _WIN32
	struct WSAGuard
	{
		WSAGuard()
		{
			// On windows we need to start WSA
			static WSADATA wsa;
			if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
			{
				mulex::LogError("WSA startup failed");
			}
			mulex::LogTrace("WSA initialized");
		}
		~WSAGuard()
		{
			WSACleanup();
			mulex::LogTrace("WSA terminated");
		}
	};
#endif

namespace mulex
{
	bool operator<(const Socket& lhs, const Socket& rhs)
	{
		return lhs._handle < rhs._handle;
	}

	Socket SocketInit()
	{
#ifdef _WIN32
		static WSAGuard _wsaguard;
#endif
		Socket socket;
		socket._error = false;
		socket._handle = ::socket(AF_INET, SOCK_STREAM, 0);
		if(socket._handle < 0)
		{
			LogError("Failed to create socket. socket returned %d", socket._handle);
			socket._error = true;
			return socket;
		}
		LogTrace("SocketInit() OK.");
		return socket;
	}

	void SocketBindListen(Socket& socket, std::uint16_t port)
	{
		sockaddr_in serveraddr;
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_port = htons(port);
		serveraddr.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0

		int binderr = ::bind(socket._handle, reinterpret_cast<sockaddr*>(&serveraddr), sizeof(serveraddr));
		if(binderr < 0)
		{
			socket._error = true;
			LogError("Failed to bind to socket. bind returned %d", binderr);
			return;
		}

		int listenerr = ::listen(socket._handle, SOMAXCONN);
		if(listenerr < 0)
		{
			socket._error = true;
			LogError("Failed to listen to socket. listen returned %d", listenerr);
			return;
		}
		LogTrace("SocketBindListen() OK.");
	}

	bool SocketSetNonBlocking(const Socket& socket)
	{
		if(socket._handle < 0) 
		{
			return false;
		}
#ifdef __linux__
		int flags = ::fcntl(socket._handle, F_GETFL, 0);
		if(flags == -1)
		{
			return false;
		}
		return (::fcntl(socket._handle, F_SETFL, flags | O_NONBLOCK) == 0);
#else
		unsigned long nonblocking = 1;
		return (::ioctlsocket(socket._handle, FIONBIO, &nonblocking) == 0);
#endif
	}

	Socket SocketAccept(const Socket& socket, bool* would_block)
	{
		Socket client;
		client._error = false;
		sockaddr_in clientaddr;
		socklen_t sz = sizeof(clientaddr);
		*would_block = false;
		client._handle = ::accept(socket._handle, reinterpret_cast<sockaddr*>(&clientaddr), &sz);
#ifdef __unix__
		if(client._handle < 0)
		{
			if((errno == EAGAIN) || (errno == EWOULDBLOCK))
			{
				*would_block = true;
				return client;
			}
			LogError("Failed to accept connection. accept returned %d", client._handle);
			client._error = true;
			return client;
		}
#else
		if(client._handle == SOCKET_ERROR)
		{
			if(WSAGetLastError() == WSAEWOULDBLOCK)
			{
				*would_block = true;
				return client;
			}
			LogError("Failed to accept connection. accept returned %d", client._handle);
			client._error = true;
			return client;
		}
#endif
		char buffer[INET_ADDRSTRLEN];
		LogDebug(
			"Got new connection from: %s:%d",
			inet_ntop(AF_INET, &clientaddr.sin_addr, buffer, INET_ADDRSTRLEN),
			ntohs(clientaddr.sin_port)
		);
		LogTrace("SocketAccept() OK.");
		return client;
	}

	SocketResult SocketRecvBytes(
		const Socket& socket,
		std::uint8_t* buffer,
		std::uint64_t len,
		std::uint64_t* rlen
	)
	{
#ifdef __unix__
			std::int64_t res = ::recv(socket._handle, buffer, len, MSG_DONTWAIT);
			if(rlen) *rlen = 0;
			if(res > 0)
			{
				if(rlen) *rlen = static_cast<std::uint64_t>(res);
				return SocketResult::OK;
			}
			else if(res < 0)
			{
				if((errno == EWOULDBLOCK) || (errno == EAGAIN))
				{
					return SocketResult::TIMEOUT;
				}

				LogError("Socket receive error.");
				return SocketResult::ERROR;
			}
			else if(res == 0)
			{
				LogWarning("Socket disconnected.");
				return SocketResult::DISCONNECT;
			}
		return SocketResult::OK;
#else
			// Windows does not have a non-blocking flag for recv only
			// And we don't want the whole socket to be non-blocking due to send
			unsigned long toread;
			::ioctlsocket(socket._handle, FIONREAD, &toread);
			if(rlen) *rlen = 0;
			if(toread > 0)
			{
				std::int32_t res = ::recv(socket._handle, reinterpret_cast<char*>(buffer), len, 0);
				if(res > 0)
				{
					if(rlen) *rlen = static_cast<std::uint64_t>(res);
					return SocketResult::OK;
				}
				else if(res < 0)
				{
					LogError("Socket receive error.");
					return SocketResult::ERROR;
				}
				else if(res == 0)
				{
					LogWarning("Socket disconnected.");
					return SocketResult::DISCONNECT;
				}
				return SocketResult::OK;
			}
			else
			{
				return SocketResult::TIMEOUT;
			}
#endif
	}

	SocketResult SocketSendBytes(const Socket& socket, std::uint8_t* buffer, std::uint64_t len)
	{
#ifdef __unix__
		int ret = ::send(socket._handle, buffer, len, MSG_NOSIGNAL);
#else
		int ret = ::send(socket._handle, reinterpret_cast<char*>(buffer), len, 0);
#endif
		if(ret < 0)
		{
			LogError("Failed to send data.");
			return SocketResult::ERROR;
		}

		return SocketResult::OK;
	}

	void SocketConnect(Socket& socket, const std::string& hostname, std::uint16_t port)
	{
		addrinfo hints;
		addrinfo* result;
		char ipv4[32]; // NOTE: 16 bytes should be enough
		char cport[8];

		::snprintf(cport, 8, "%d", port);

		std::memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		int error = ::getaddrinfo(hostname.c_str(), cport, &hints, &result);
		if(error != 0)
		{
			LogError("getaddrinfo failed with error: %d", errno);
			LogError(gai_strerror(error));
			socket._error = true;
			return;
		}

		inet_ntop(
			result->ai_family,
			&reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr,
			ipv4,
			32
		);

		LogDebug("Trying connection to: %s", ipv4);

		for(addrinfo* p = result; p != nullptr; p = p->ai_next)
		{
			if(::connect(socket._handle, p->ai_addr, p->ai_addrlen) < 0)
			{
				socket._error = true;
				LogError("Failed to connect.");
				LogMessage("Trying next node.");
				continue;
			}

			socket._error = false;
			LogDebug("Connection established");
			break;
		}

		::freeaddrinfo(result);
	}

	void SocketClose(Socket& socket)
	{
#ifdef __unix__
		::close(socket._handle);
#else
		::closesocket(socket._handle);
#endif
	}

} // namespace mulex
