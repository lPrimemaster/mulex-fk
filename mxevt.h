#pragma once
#include <cstdint>
#include <string>
#include <thread>
#include <map>
#include <set>

#include "mxtypes.h"
#include "network/socket.h"
#include "network/rpc.h"
#include "mxsystem.h"

namespace mulex
{
	static constexpr std::uint16_t EVT_PORT = 5702;
	static constexpr std::uint16_t EVT_RECV_TIMEOUT = 10000; // 10 sec

	enum class EvtResult
	{
		OK,
		FAILED,
		TIMEOUT
	};
	
	struct EvtHeader
	{
		std::uint64_t client;
		std::uint16_t eventid;
		std::uint64_t msgid;
		std::uint32_t payloadsize;
		// std::uint8_t  padding[10];
	};

	constexpr std::uint64_t EVT_HEADER_SIZE = sizeof(EvtHeader);
	constexpr std::uint64_t EVT_MAX_SUB = 64;

	struct EvtStatistics
	{
		std::uint64_t							_len = 0;
		std::vector<string32> 	 				_name;
		std::vector<std::uint64_t> 				_total_frames;
		std::vector<std::vector<std::uint64_t>>	_clients;
		std::vector<std::vector<std::uint64_t>> _frames;
	};

	std::uint64_t GetNextEventMessageId();

	class EvtClientThread
	{
	public:
		using EvtCallbackFunc = std::function<void(const std::uint8_t* data, std::uint64_t len, const std::uint8_t* userdata)>;
		EvtClientThread(
			const std::string& hostname,
			const Experiment* exp = nullptr,
			std::uint16_t evtport = EVT_PORT,
			bool ghost = false,
			std::uint64_t customid = 0x0
		);
		~EvtClientThread();

		void emit(const std::string& event, const std::uint8_t* data, std::uint64_t len);
		void regist(const std::string& event);
		void subscribe(const std::string& event, EvtCallbackFunc callback);
		void unsubscribe(const std::string& event);
		void unsubscribeAll();
		std::uint16_t findEvent(const std::string& event);

	private:
		void clientListenThread(const Socket& socket);
		void clientEmitThread(const Socket& socket);

	private:
		const Experiment* _exp;
		Socket _evt_socket;
		std::unique_ptr<std::thread> _evt_listen_thread;
		std::unique_ptr<std::thread> _evt_emit_thread;
		SysByteStream* _evt_stream;
		SysBufferStack _evt_emit_stack;
		std::atomic<bool> _evt_thread_running = false;
		std::atomic<bool> _evt_thread_ready = false;
		std::map<std::string, std::uint16_t> _evt_registry;
		std::set<std::string> _evt_subscriptions;
		std::map<std::uint16_t, EvtCallbackFunc> _evt_callbacks;
		std::map<std::uint16_t, std::uint8_t*> _evt_userdata;
		bool _evt_has_custom_id = false;
		std::uint64_t _evt_custom_id;
	};

	class EvtServerThread
	{
	public:
		EvtServerThread();
		~EvtServerThread();
		bool ready() const;

		bool emit(const std::string& event, const std::uint8_t* data, std::uint64_t len);
		void relay(const std::uint64_t clientid, const std::uint8_t* data, std::uint64_t len);
		void unsub(const std::uint64_t cid);

	private:
		void serverConnAcceptThread();
		void serverListenThread(const Socket& socket);
		void serverEmitThread(const Socket& socket);
		void clientStatisticsThread();

	private:
		Socket _server_socket;
		std::map<Socket, std::unique_ptr<std::thread>> _evt_listen_thread;
		std::map<Socket, std::unique_ptr<std::thread>> _evt_emit_thread;
		std::map<Socket, SysByteStream*> _evt_stream;
		std::map<Socket, std::atomic<bool>> _evt_thread_sig;
		std::map<Socket, SysBufferStack> _evt_emit_stack;
		// SysRefBufferStack _evt_emit_stack;
		std::unique_ptr<std::thread> _evt_accept_thread;
		std::unique_ptr<std::thread> _evt_stats_thread;
		std::atomic<bool> _evt_thread_running = false;
		std::atomic<bool> _evt_thread_ready = false;
		std::mutex _connections_mutex;
		std::condition_variable _evt_notifier;
	};

	MX_RPC_METHOD bool EvtRegister(mulex::string32 name);
	MX_RPC_METHOD std::uint16_t EvtGetId(mulex::string32 name);
	MX_RPC_METHOD bool EvtSubscribe(mulex::string32 name);
	MX_RPC_METHOD bool EvtUnsubscribe(mulex::string32 name);
	bool EvtUnsubscribe(std::uint64_t clientid, std::uint16_t eventid);
	void EvtServerRegisterCallback(mulex::string32 name, std::function<void(const Socket&, std::uint64_t, std::uint16_t, const std::uint8_t*, std::uint64_t)> callback);
	void EvtTryRunServerCallback(std::uint64_t clientid, std::uint16_t eventid, const std::uint8_t* data, std::uint64_t len, const Socket& socket);
	bool EvtEmit(const std::string& event, const std::uint8_t* data, std::uint64_t len);
	void EvtAccumulateClientStatistics(std::uint64_t clientid, std::uint64_t framebytes);
	void EvtAccumulateEventStatistics(std::uint16_t eventid, std::uint64_t clientid, std::uint64_t framebytes, bool create = false);
	void EvtResetStatistics(std::uint16_t eventid);
	void EvtMakeStats(const mulex::string32& name, std::uint16_t eventid);
	void EvtMakeStatsEntry(std::uint16_t eventid, std::uint64_t clientid);
	void EvtDeleteStatsEntry(std::uint16_t eventid, std::uint64_t clientid);
	void EvtPurgeStatsEntryClient(std::uint64_t clientid);
	std::uint64_t EvtCalculateStatisticsBufferSize();

	MX_RPC_METHOD mulex::RPCGenericType EvtGetAllRegisteredEvents();
	MX_RPC_METHOD mulex::RPCGenericType EvtGetAllMetadata();

	template <typename T>
	inline std::uint64_t EvtDataAppend(std::uint64_t offset, std::vector<std::uint8_t>* buffer, const T& value)
	{
		std::memcpy(buffer->data() + offset, &value, sizeof(T));
		return offset + sizeof(T);
	}

	inline std::uint64_t EvtDataAppend(std::uint64_t offset, std::vector<std::uint8_t>* buffer, const std::uint8_t* value, std::uint64_t len)
	{
		std::memcpy(buffer->data() + offset, value, len);
		return offset + len;
	}
} // namespace mulex
