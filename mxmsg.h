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

	class MsgEmitter
	{
	public:
		explicit MsgEmitter(const std::string& name);
		virtual ~MsgEmitter() {  }
		
		void emitMessageInfo(const char* fmt, ...);
		void emitMessageWarn(const char* fmt, ...);
		void emitMessageError(const char* fmt, ...);

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

} // namespace mulex
