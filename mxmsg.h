#pragma once
#include <string>
#include "network/rpc.h"

namespace mulex
{
	enum class MsgClass : std::uint8_t
	{
		INFO,
		WARN,
		ERROR
	};

	static constexpr std::uint64_t MSG_MAX_SIZE = 1024;

	struct MsgMessageHeader
	{
		std::uint64_t _client;
		std::int64_t  _timestamp;
		MsgClass 	  _type;
		std::uint8_t  _padding[7];
		std::uint64_t _size;
	};

	class MsgEmitter
	{
	public:
		explicit MsgEmitter(const std::string& name = "");
		virtual ~MsgEmitter() {  }
		
		void info(const char* fmt, ...);
		void warn(const char* fmt, ...);
		void error(const char* fmt, ...);

		void attachLogger(bool attach);

	private:
		// NOTE: (Cesar) C-style varargs allows to *easily* 
		// 				 move the implementation to a cpp file.
		std::string emitMessage(const char* fmt, MsgClass mclass, va_list vargs);
		
	private:
		bool _attachlog;
	};

	std::string_view MsgClassToString(MsgClass mclass);
	MX_RPC_METHOD void MsgWrite(mulex::MsgClass mclass, std::int64_t timestamp, mulex::RPCGenericType msg);
	MX_RPC_METHOD mulex::RPCGenericType MsgGetLastLogs(std::uint32_t count);
	MX_RPC_METHOD mulex::RPCGenericType MsgGetLastClientLogs(std::uint64_t cid, std::uint32_t count);
	void MsgInit();
	void MsgClose();

} // namespace mulex
