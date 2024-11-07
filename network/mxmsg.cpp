#include "../mxmsg.h"
#include "../mxsystem.h"
#include "../mxlogger.h"

#include <cstdarg>
#include <vector>
#include <sstream>

#include <rpcspec.inl>

namespace mulex
{
	MsgEmitter::MsgEmitter(const std::string& name) : _attachlog(true)
	{
	}
	
	void MsgEmitter::emitMessageInfo(const char* fmt, ...)
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

	void MsgEmitter::emitMessageWarn(const char* fmt, ...)
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

	void MsgEmitter::emitMessageError(const char* fmt, ...)
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
			LogWarning("emitMessage failed, not connected to an experiment.");
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
		const std::uint16_t cid = GetCurrentCallerId();

		// Write message rdb stats
		std::string rkey = "/system/clients/" + std::to_string(cid) + "/msg";
		RdbEntry* entry = RdbFindEntryByName(rkey + "/count");
		if(entry)
		{
			std::uint64_t count = RdbReadValueDirect(rkey + "/count");
			RdbWriteValueDirect(rkey + "/count", count + 1);
		}
		else
		{
			std::uint64_t count = 1;
			RdbNewEntry(rkey + "/count", RdbValueType::UINT64, &count);
		}

		entry = RdbFindEntryByName(rkey + "/last");
		if(entry)
		{
			RdbWriteValueDirect(rkey + "/last", msg);
		}
		else
		{
			RdbNewEntry(rkey + "/last", RdbValueType::STRING, msg.getData());
		}
		
		// TODO: (Cesar) Get caller name (TODO register callers)
		//				 Log the message on the pdb and trigger event 
		//				 TODO - rdb Events ? / msg events ?
	}
} // namespace mulex
