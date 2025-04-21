#include "../mxmsg.h"
#include "../mxsystem.h"
#include "../mxlogger.h"
#include "../mxrdb.h"
#include "rpc.h"

#include <cstdarg>
#include <vector>
#include <sstream>
#include <queue>
#include <rpcspec.inl>

static std::queue<std::tuple<std::uint8_t, std::uint64_t, std::int64_t, mulex::PdbString>> _msg_queue;
static std::mutex _msg_queue_lock;
static constexpr std::uint64_t MSG_MAX_BACKLOG = 20;

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

	static void MsgFlushSQLQueue()
	{
		ZoneScoped;
		LogTrace("[mxmsg] Flushing logs to disk.");
		while(!_msg_queue.empty())
		{
			const static std::vector<PdbValueType> types = { PdbValueType::NIL, PdbValueType::UINT8, PdbValueType::UINT64, PdbValueType::INT64, PdbValueType::STRING };
			auto [level, client, timestamp, message] = _msg_queue.front();
			_msg_queue.pop();
			std::vector<std::uint8_t> data = SysPackArguments(level, client, timestamp, message);
			PdbWriteTable("INSERT INTO logs (id, level, client, timestamp, message) VALUES (?, ?, ?, ?, ?);", types, data);
		}
	}

	void MsgWrite(mulex::MsgClass mclass, std::int64_t timestamp, mulex::RPCGenericType msg)
	{
		ZoneScoped;
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
		// LogTrace("Local buffer: %s", message_data.data());
		// LogTrace("Local size: %llu", msg.getSize());

		EvtEmit("mxmsg::message", buffer, sizeof(MsgMessageHeader) + message_size);

		// Now place the message on the pdb queue
		std::unique_lock<std::mutex> lock(_msg_queue_lock);
		_msg_queue.push({
			static_cast<std::uint8_t>(message->_type),
			message->_client,
			message->_timestamp,
			PdbString(reinterpret_cast<const char*>(buffer + sizeof(MsgMessageHeader)))
		});

		if(_msg_queue.size() > MSG_MAX_BACKLOG)
		{
			// No need to defer
			// MsgFlushSQLQueue would block any MsgWrite call anyways
			MsgFlushSQLQueue();
		}
	}

	mulex::RPCGenericType MsgGetLastLogs(std::uint32_t count)
	{
		ZoneScoped;
		std::unique_lock<std::mutex> lock(_msg_queue_lock);

		// Force queue flush when getting last logs
		MsgFlushSQLQueue();

		const static std::vector<PdbValueType> types = { PdbValueType::INT32, PdbValueType::UINT8, PdbValueType::UINT64, PdbValueType::INT64, PdbValueType::STRING };
		return PdbReadTable("SELECT * FROM logs ORDER BY id DESC LIMIT " + std::to_string(count) + ";", types);
	}

	void MsgInit()
	{
		ZoneScoped;
		if(!EvtRegister("mxmsg::message"))
		{
			return;
		}

		const std::string query = 
		"CREATE TABLE IF NOT EXISTS logs ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"level INTEGER NOT NULL,"
			"client BIGINT NOT NULL,"
			"timestamp BIGINT NOT NULL,"
			"message TEXT NOT NULL"
		");";
		PdbExecuteQuery(query);

		LogTrace("[mxmsg] MsgInit() OK.");
	}

	void MsgClose()
	{
		ZoneScoped;
		std::unique_lock<std::mutex> lock(_msg_queue_lock);
		MsgFlushSQLQueue();
	}
} // namespace mulex
