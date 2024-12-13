#include "../mxmsg.h"
#include "../mxsystem.h"
#include "../mxlogger.h"
#include "../mxrdb.h"
#include "rpc.h"

#include <cstdarg>
#include <vector>
#include <sstream>
#include <rpcspec.inl>

namespace mulex
{
	MsgEmitter::MsgEmitter(const std::string& name) : _attachlog(true)
	{
	}
	
	void MsgEmitter::info(const char* fmt, ...)
	{
		va_list vargs;
		va_start(vargs, fmt);
		std::string msgdata = emitMessage(fmt, MsgClass::INFO, vargs);
		va_end(vargs);

		if(_attachlog)
		{
			LogMessage("%s", msgdata.c_str());
		}
	}

	void MsgEmitter::warn(const char* fmt, ...)
	{
		va_list vargs;
		va_start(vargs, fmt);
		std::string msgdata = emitMessage(fmt, MsgClass::WARN, vargs);
		va_end(vargs);

		if(_attachlog)
		{
			LogWarning("%s", msgdata.c_str());
		}
	}

	void MsgEmitter::error(const char* fmt, ...)
	{
		va_list vargs;
		va_start(vargs, fmt);
		std::string msgdata = emitMessage(fmt, MsgClass::ERROR, vargs);
		va_end(vargs);

		if(_attachlog)
		{
			LogError("%s", msgdata.c_str());
		}
	}

	void MsgEmitter::attachLogger(bool attach)
	{
		_attachlog = attach;
	}

	std::string MsgEmitter::emitMessage(const char* fmt, MsgClass mclass, va_list vargs)
	{
		std::int64_t time = SysGetCurrentTime();
		va_list vargscpy;
		va_copy(vargscpy, vargs);

		int vsize = vsnprintf(nullptr, 0, fmt, vargs);
		std::vector<char> buffer;
		buffer.resize(vsize + 1);
		vsnprintf(buffer.data(), vsize + 1, fmt, vargscpy);
		va_end(vargscpy);

		std::stringstream ss;
		ss << buffer.data();
		std::string output = ss.str();

		std::optional<const Experiment*> experiment = SysGetConnectedExperiment();
		if(!experiment.has_value())
		{
			LogWarning("[mxmsg] emitMessage failed, not connected to an experiment.");
		}
		else
		{
			experiment.value()->_rpc_client->call(RPC_CALL_MULEX_MSGWRITE, mclass, time, RPCGenericType(buffer));
		}
		return output;
	}

	std::string_view MsgClassToString(MsgClass mclass)
	{
		switch(mclass)
		{
			case MsgClass::INFO:  return "[INFO]";
			case MsgClass::WARN:  return "[WARN]";
			case MsgClass::ERROR: return "[ERROR]";
		}
		return "";
	}

	void MsgWrite(mulex::MsgClass mclass, std::int64_t timestamp, mulex::RPCGenericType msg)
	{
		const std::vector<char> message_data = msg.asVectorType<char>();
		const std::uint64_t message_size = message_data.size() < MSG_MAX_SIZE ? message_data.size() : MSG_MAX_SIZE;

		std::uint8_t buffer[sizeof(MsgMessageHeader) + MSG_MAX_SIZE];
		MsgMessageHeader* message = reinterpret_cast<MsgMessageHeader*>(&buffer);

		// TODO: (Cesar) This client id is duplicated (already exists on the evt header)
		message->_client = GetCurrentCallerId();
		message->_timestamp = timestamp;
		message->_type = mclass;
		message->_size = message_size;
		std::memcpy(buffer + sizeof(MsgMessageHeader), message_data.data(), message_size);
		LogTrace("Local buffer: %s", message_data.data());
		LogTrace("Local size: %llu", msg.getSize());

		EvtEmit("mxmsg::message", buffer, sizeof(MsgMessageHeader) + message_size);

		// Now place the message on the pdb
		// PdbWriteTable();
	}

	void MsgInit()
	{
		if(!EvtRegister("mxmsg::message"))
		{
			return;
		}
		LogTrace("[mxmsg] MsgInit() OK.");
	}
} // namespace mulex
