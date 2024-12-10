#pragma once
#include "mxevt.h"
#include "mxrdb.h"
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
		void registerEvent(const std::string& evt);
		void subscribeEvent(const std::string& evt, EvtClientThread::EvtCallbackFunc func);

	protected:
		virtual void onRunStart(std::uint64_t runno);
		virtual void onRunStop(std::uint64_t runno);
		void deferExec(std::function<void()> wrapfunc);

	public:
		void startEventLoop();
		void eventLoop();

	protected:
		RdbAccess rdb;

	private:
		// Init metadata
		bool _init_ok = false;
		const Experiment* _experiment = nullptr;

		// Periodic polling
		std::int32_t _period_ms;
	};
} // namespace mulex
