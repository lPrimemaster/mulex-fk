#pragma once
#include "mxevt.h"
#include "mxrdb.h"
#include "mxmsg.h"
#include <string>

namespace mulex
{
	class MxBackend
	{
	protected:
		MxBackend(int argc, char* argv[]);
		virtual ~MxBackend();
		virtual void periodic() {  };

		// Events
		void dispatchEvent(const std::string& evt, const std::uint8_t* data, std::uint64_t size);
		void dispatchEvent(const std::string& evt, const std::vector<std::uint8_t>& data);
		void registerEvent(const std::string& evt);
		void subscribeEvent(const std::string& evt, EvtClientThread::EvtCallbackFunc func);
		void unsubscribeEvent(const std::string& evt);

		// Event helper

	protected:
		virtual void onRunStart(std::uint64_t runno);
		virtual void onRunStop(std::uint64_t runno);
		void deferExec(std::function<void()> wrapfunc);

	public:
		void startEventLoop();
		void eventLoop();

	protected:
		RdbAccess rdb;
		PdbAccessRemote pdb;
		MsgEmitter log;

	private:
		// Init metadata
		bool _init_ok = false;
		const Experiment* _experiment = nullptr;

		// Periodic polling
		std::int32_t _period_ms;
	};

	template<typename T>
	concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

	class MxEventBuilder
	{
	public:
		MxEventBuilder(std::vector<std::uint8_t>& buffer) : _buffer_ref(buffer) {  };

		template<TriviallyCopyable T>
		MxEventBuilder& add(const T& t)
		{
			const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(&t);
			_buffer_ref.insert(_buffer_ref.end(), ptr, ptr + sizeof(T));
			return *this;
		}

		template<TriviallyCopyable T>
		MxEventBuilder& add(const std::vector<T>& t)
		{
			_buffer_ref.insert(_buffer_ref.end(), t.begin(), t.end());
			return *this;
		}

		operator const std::vector<std::uint8_t>&() { return _buffer_ref; }

	private:
		std::vector<std::uint8_t>& _buffer_ref;
	};

} // namespace mulex
