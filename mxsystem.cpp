#include "mxsystem.h"
#include "rpc/rpc.h"

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
		_sys_experiment._exp_socket = SocketInit();
		SocketConnect(_sys_experiment._exp_socket, hostname, port);

		_sys_experiment._rpc_socket = SocketInit();
		SocketConnect(_sys_experiment._rpc_socket, hostname, RPC_PORT);

		// TODO: Get other experiment data to populate
		// 		 the struct here after connection

		_sys_experiment_connected = !(
			_sys_experiment._exp_socket._error ||
			_sys_experiment._rpc_socket._error
		);
		return _sys_experiment_connected;
	}

	void SysDisconnectFromExperiment()
	{
		if(_sys_experiment._exp_socket._handle >= 0)
		{
			SocketClose(_sys_experiment._exp_socket);
		}

		if(_sys_experiment._rpc_socket._handle >= 0)
		{
			SocketClose(_sys_experiment._rpc_socket);
		}
		
		_sys_experiment_connected = false;
	}
} // namespace mulex
