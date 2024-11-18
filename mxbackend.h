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

		// Rdb
		RdbAccess getConfigRdbRoot();

	public:
		void startEventLoop();
		void eventLoop();

	private:
		bool _init_ok = false;
		const Experiment* _experiment = nullptr;

	protected:
		std::int32_t _period_ms;
		RdbAccess rdb;
	};
} // namespace mulex
