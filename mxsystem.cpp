#include "mxsystem.h"
#include <signal.h>
#include "rpc/rpc.h"

// static mulex::RPCClientThread _sys_rct;
static mulex::Experiment _sys_experiment;
static bool _sys_experiment_connected = false;

namespace mulex
{
	std::optional<const Experiment*> SysGetConnectedExperiment()
	{
		if(_sys_experiment_connected)
			return &_sys_experiment;
		return std::nullopt;
	}

	bool SysConnectToExperiment(const char* hostname, std::uint16_t port)
	{
		_sys_experiment._rpc_client = std::make_unique<RPCClientThread>(hostname, RPC_PORT);
		_sys_experiment_connected = true;
		return true;
	}

	void SysDisconnectFromExperiment()
	{
		// if(_sys_experiment._exp_socket._handle >= 0)
		// {
		// 	SocketClose(_sys_experiment._exp_socket);
		// }
		//
		// if(_sys_experiment._rpc_socket._handle >= 0)
		// {
		// 	SocketClose(_sys_experiment._rpc_socket);
		// }
		
		if(_sys_experiment._rpc_client)
		{
			_sys_experiment._rpc_client.reset();
		}
		
		_sys_experiment_connected = false;
	}

	void SysRegisterSigintAction(SysSigintActionFunc f)
	{
		::signal(SIGINT, f);
	}

	SysRecvThread::SysRecvThread(const Socket& socket, std::uint64_t ssize, std::uint64_t sheadersize, std::uint64_t sheaderoffset)
		: _handle(), _stream(ssize, sheadersize, sheaderoffset)
	{
		_handle = std::thread([&](){
			static constexpr std::uint64_t SOCKET_RECV_BUFSIZE = 32768;
			std::uint8_t rbuffer[SOCKET_RECV_BUFSIZE];
			std::uint64_t read;
			while(true)
			{
				SocketResult r = SocketRecvBytes(socket, rbuffer, SOCKET_RECV_BUFSIZE, &read);
				if((r == SocketResult::DISCONNECT) || (r == SocketResult::ERROR))
				{
					break;
				}
				else if(r == SocketResult::TIMEOUT)
				{
					if(_stream.unblockRequested())
					{
						break;
					}
					std::this_thread::sleep_for(std::chrono::microseconds(10));
					continue;
				}

				if(!_stream.push(rbuffer, read))
				{
					break;
				}
			}
			LogTrace("SysRecvThread: Stopped.");
		});
	}
	
	[[nodiscard]] std::unique_ptr<SysRecvThread> SysStartRecvThread(const Socket& socket, std::uint64_t headersize, std::uint64_t headeroffset)
	{
		if(SysRecvThreadCanStart(socket))
		{
			return std::make_unique<SysRecvThread>(socket, SYS_RECV_THREAD_BUFFER_SIZE, headersize, headeroffset);
		}

		LogError("SysStartRecvThread failed to start. Provided socket already has a running recv thread.");
		return nullptr;
	}

	bool SysRecvThreadCanStart(const Socket& socket)
	{
		static_cast<void>(socket);
		return true;
	}


	void SysBufferStack::push(std::vector<std::uint8_t>&& data)
	{
		{
			std::unique_lock<std::mutex> lock(_mutex);
			_stack.push(std::move(data));
		}
		_notifier.notify_one();	
	}

	std::vector<std::uint8_t> SysBufferStack::pop()
	{
		std::unique_lock<std::mutex> lock(_mutex);
		_notifier.wait(lock);
		std::vector<std::uint8_t> val = std::move(_stack.top());
		_stack.pop();
		return val;
	}

	SysByteStream::SysByteStream(std::uint64_t size, std::uint64_t headersize, std::uint64_t headeroffset)
	{
		_buffer.resize(size);
		_buffer_offset = 0;
		_header_size = headersize;
		_header_size_offset = headeroffset;
		_unblock_sig.store(false);
	}

	bool SysByteStream::push(std::uint8_t* data, std::uint64_t size)
	{
		{
			std::unique_lock<std::mutex> lock(_mutex);
			
			_notifier.wait(lock, [&](){ return (_buffer_offset + size <= _buffer.size()) || _unblock_sig.load(); });

			if(_unblock_sig.load())
			{
				return false;
			}
			
			std::memcpy(_buffer.data() + _buffer_offset, data, size);
			_buffer_offset += size;
		}
		_notifier.notify_one();
		return true;
	}

	std::uint64_t SysByteStream::fetch(std::uint8_t* buffer, std::uint64_t size)
	{
		std::uint64_t payloadsize = 0;
		{
			std::unique_lock<std::mutex> lock(_mutex);
			
			_notifier.wait(lock, [&](){ return (_buffer_offset >= _header_size) || _unblock_sig.load(); });
			
			if(_unblock_sig.load())
			{
				return 0;
			}
			
			std::uint32_t msg_size;
			std::memcpy(&msg_size, _buffer.data() + _header_size_offset, sizeof(std::uint32_t));
			
			payloadsize = msg_size + _header_size;
			
			_notifier.wait(lock, [&](){ return payloadsize <= _buffer_offset; });

			std::memcpy(buffer, _buffer.data(), payloadsize);
			_buffer_offset -= payloadsize;

			if(_buffer_offset > 0)
			{
				std::memmove(_buffer.data(), _buffer.data() + payloadsize, _buffer_offset);
			}
		}
		_notifier.notify_one();
		return payloadsize;
	}

	void SysByteStream::requestUnblock()
	{
		_unblock_sig.store(true);
		_notifier.notify_all();
	}

	const bool SysByteStream::unblockRequested() const
	{
		return _unblock_sig.load();
	}

	std::int64_t SysGetCurrentTime()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}
} // namespace mulex
