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

#include <thread>
#include <cstring>
#include <chrono>

namespace mulex
{
	bool operator<(const Socket& lhs, const Socket& rhs)
	{
		return lhs._handle < rhs._handle;
	}

	Socket SocketInit()
	{
		Socket socket;
		socket._error = false;
#ifdef __unix__
		socket._handle = ::socket(AF_INET, SOCK_STREAM, 0);
		if(socket._handle < 0)
		{
			LogError("Failed to create socket. socket returned %d", socket._handle);
			socket._error = true;
			return socket;
		}
#else
#endif
		LogTrace("SocketInit() OK.");
		return socket;
	}

	void SocketBindListen(Socket& socket, std::uint16_t port)
	{
#ifdef __unix__
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
#else
#endif
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
#endif
	}

	Socket SocketAccept(const Socket& socket, bool* would_block)
	{
		Socket client;
		client._error = false;
#ifdef __unix__
		sockaddr_in clientaddr;
		socklen_t sz = sizeof(clientaddr);
		*would_block = false;
		client._handle = ::accept(socket._handle, reinterpret_cast<sockaddr*>(&clientaddr), &sz);
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
		
		char buffer[INET_ADDRSTRLEN];
		LogDebug(
			"Got new connection from: %s:%d",
			inet_ntop(AF_INET, &clientaddr.sin_addr, buffer, INET_ADDRSTRLEN),
			ntohs(clientaddr.sin_port)
		);
#else
#endif
		LogTrace("SocketAccept() OK.");
		return client;
	}

	SocketResult SocketRecvBytes(
		const Socket& socket,
		std::uint8_t* buffer,
		std::uint64_t len,
		std::atomic<bool>* notify_unblock,
		std::uint32_t timeout_ms
	)
	{
		// Setup timeout clock
		std::chrono::time_point<std::chrono::steady_clock> srb_start;
		if(timeout_ms > 0)
		{
			srb_start = std::chrono::steady_clock::now();
		}

		// Expect the len
		while(len > 0)
		{
#ifdef __unix__
			int res = ::recv(socket._handle, buffer, len, MSG_DONTWAIT);
			if(res > 0)
			{
				buffer += res;
				len -= res;
			}
			else if(res < 0)
			{
				if((errno == EWOULDBLOCK) || (errno == EAGAIN))
				{
					// TODO: use select();
					// For now just yield for a while and retry
					// After just put in a select thread spinning
					// for all the sockets and condition variables
					if(!notify_unblock && timeout_ms == 0)
					{
						LogError("For no unblocker, it is required to specify a non zero timeout.");
						return SocketResult::ERROR;
					}
					if(timeout_ms > 0)
					{
						if(std::chrono::steady_clock::now() - srb_start > std::chrono::milliseconds(timeout_ms))
						{
							LogTrace("SocketRecvBytes() timeout.");
							return SocketResult::TIMEOUT;
						}
					}
					if(notify_unblock && !notify_unblock->load())
					{
						LogTrace("SocketRecvBytes() notified to exit. Returning.");
						return SocketResult::DISCONNECT;
					}
					std::this_thread::sleep_for(std::chrono::microseconds(100));
				}
				else
				{
					LogError("Socket receive error.");
					return SocketResult::ERROR;
				}
			}
			else if(res == 0)
			{
				return SocketResult::DISCONNECT;
			}
		}
#else
#endif
		return SocketResult::OK;
	}

	SocketResult SocketSendBytes(const Socket& socket, std::uint8_t* buffer, std::uint64_t len)
	{
#ifdef __unix__
		int ret = ::send(socket._handle, buffer, len, MSG_NOSIGNAL);
#else
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
#endif
	}

} // namespace mulex
