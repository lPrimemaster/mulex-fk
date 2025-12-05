import { createStore, produce } from "solid-js/store";
import { MxGenericType } from "./convert";
import { MxWebsocket } from "./websocket";
import { MxRdb } from "./rdb";
import { extract_backend_name } from "./utils";
import { createSignal } from "solid-js";
import { showToast } from "~/components/ui/toast";

interface BackendStatus {
	name: string;
	host: string;
	connected: boolean;
	evt_upload_speed: number;
	evt_download_speed: number;
	uptime: number;
	user_status: string;
	user_color: string;
};

interface BackendStatusList {
	[key: string]: BackendStatus;
};

interface UserRole {
	name: string;
	id: number;
};

// Global signals/stores
const [backends, setBackends] = createStore<BackendStatusList>();
const [loggedUser, setLoggedUser] = createSignal<string>('');
const [loggedUserRole, setLoggedUserRole] = createSignal<UserRole>({ name: 'none', id: -1 });
const [expname, setExpname] = createSignal<string>('');
const [socketStatus, setSocketStatus] = createSignal<boolean>(true);
const [runStatus, setRunStatus] = createSignal<string>('Stopped');
const [runNumber, setRunNumber] = createSignal<number>(0);
const [runTimestamp, setRunTimestamp] = createSignal<number>(0);

export {
	// Global state variables
	// This data is always persistent through the entire frontend
	// Would be possible to move all this to a solidjs context
	// and use it on the root file. However I decided to use this
	// and I remember reasoning about it, but now I don't know why...
	
	// Backends
	backends 	   as gBackends,

	// Users
	loggedUser 	   as gLoggedUser,
	loggedUserRole as gLoggedUserRole,

	// Experiment
	expname 	   as gExpname,

	// Socket
	socketStatus   as gSocketStatus,

	// Run data
	runStatus	   as gRunStatus,
	runNumber	   as gRunNumber,
	runTimestamp   as gRunTimestamp,

	// Global state init funtion
	init_global_state as initGlobalState
};

async function create_client_status(clientid: string): Promise<BackendStatus> {
	let entry = MxGenericType.str512(`/system/backends/${clientid}/name`);
	const cname: string = (await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [entry], 'generic')).astype('string');
	entry = MxGenericType.str512(`/system/backends/${clientid}/host`);
	const chost: string = (await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [entry], 'generic')).astype('string');
	entry = MxGenericType.str512(`/system/backends/${clientid}/connected`);
	const cconn: boolean = (await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [entry], 'generic')).astype('bool');
	entry = MxGenericType.str512(`/system/backends/${clientid}/last_connect_time`);
	const ctime: number = (await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [entry], 'generic')).astype('int64');
	entry = MxGenericType.str512(`/system/backends/${clientid}/user_status/text`);
	const ustatus: string = (await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [entry], 'generic')).astype('string');
	entry = MxGenericType.str512(`/system/backends/${clientid}/user_status/color`);
	const ucolor: string = (await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [entry], 'generic')).astype('string');

	return {
		name: cname,
		host: chost,
		connected: cconn,
		uptime: Number(ctime),
		user_status: ustatus,
		user_color: ucolor,
		evt_upload_speed: 0,
		evt_download_speed: 0
	};
}

async function init_client_status() {
	const data = await MxWebsocket.instance.rpc_call('mulex::RdbListSubkeys', [MxGenericType.str512('/system/backends/*/name')], 'generic');

	const client_status: Array<string> = data.astype('stringarray');
	client_status
		.map((x) => {
			// Return the client id
			return extract_backend_name(x);
		})
		.forEach((x) => {
			create_client_status(x).then((status: BackendStatus) => {
				setBackends(x, () => status);
			});
		});
}

async function fetch_client_name() {
	// Set current logged username
	const res = await fetch('/api/auth/username', {
		method: 'POST'
	});

	const data = await res.json();
	setLoggedUser(data.return);
}

async function fetch_client_role() {
	const role = await MxWebsocket.instance.rpc_call('mulex::PdbUserGetCurrentRole', [], 'generic');
	const [id, name] = role.unpack(['int32', 'str512'])[0];
	setLoggedUserRole({ name, id });
}

async function fetch_experiment_name() {
	const name = await MxWebsocket.instance.rpc_call('mulex::SysGetExperimentName');
	setExpname(name.astype('string'));
}

function setRunStatusFromCode(code: number) {
	switch(code) {
		case 0: setRunStatus('Stopped'); break;
		case 1: setRunStatus('Running'); break;
		case 2: setRunStatus('Starting'); break;
		case 3: setRunStatus('Stopping'); break;
	}
}

async function init_run_status() {
	const rdb = new MxRdb();

	rdb.watch('/system/run/status', (_: string, value: MxGenericType) => {
		setRunStatusFromCode(value.astype('uint8'));
		if(value.astype('uint8') == 1) {
			showToast({ title: 'Run ' + runNumber() + ' started.', variant: 'success'});
		}
		else if(value.astype('uint8') == 0) {
			showToast({ title: 'Run ' + runNumber() + ' stopped.', variant: 'error'});
		}
	});

	rdb.watch('/system/run/number', (_: string, value: MxGenericType) => {
		setRunNumber(Number(value.astype('uint64')));
	});

	rdb.watch('/system/run/timestamp', (_: string, value: MxGenericType) => {
		setRunTimestamp(Number(value.astype('int64')));
	});

	MxWebsocket.instance.on_connection_change((conn: boolean) => {
		setSocketStatus(conn);
	});

	MxWebsocket.instance.rpc_call(
		'mulex::RdbReadValueDirect',
		[MxGenericType.str512('/system/run/status')],
		'generic').then((res: MxGenericType) => {
			const rstatus = res.astype('uint8');
			// Status MEMO:
			// 0 - Stopped
			// 1 - Running
			// 2 - Starting
			// 3 - Stopping
			setRunStatusFromCode(rstatus);
	});

	MxWebsocket.instance.rpc_call(
		'mulex::RdbReadValueDirect',
		[MxGenericType.str512('/system/run/number')],
		'generic').then((res: MxGenericType) => {
			const rnumber = res.astype('uint64');
			setRunNumber(Number(rnumber));
	});

	MxWebsocket.instance.rpc_call(
		'mulex::RdbReadValueDirect',
		[MxGenericType.str512('/system/run/timestamp')],
		'generic').then((res: MxGenericType) => {
			const rts = res.astype('int64');
			setRunTimestamp(Number(rts));
	});
}

async function init_backend_status() {
	const rdb = new MxRdb();

	// Connection status
	rdb.watch('/system/backends/*/connected', async (key: string, value: MxGenericType) => {
		const cid = extract_backend_name(key);
		const connected = value.astype('bool');
		const user_status = 'None';

		const conkey = '/system/backends/' + cid + '/last_connect_time';
		const res = await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(conkey)], 'generic');
		const uptime = Number(res.astype('int64'));
		setBackends(cid, {
			connected,
			user_status,
			uptime
		});
	});

	// Backend creation
	rdb.watch('/system/backends/*/name', async (key: string, value: MxGenericType) => {
		const cid = extract_backend_name(key);
		const name = value.astype('string');

		const hostkey = '/system/backends/' + cid + '/host';
		const res = await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(hostkey)], 'generic');
		const host = res.astype('string');
		setBackends(cid, {
			name,
			host
		});
	});

	// Connection metrics
	rdb.watch('/system/backends/*/statistics/event/*', async (key: string) => {
		const res = await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(key)], 'generic');
		const cid = extract_backend_name(key);
		const speed = res.astype('uint32');
		if(key.endsWith('read')) {
			setBackends(cid, { evt_download_speed: speed });
		}
		else if(key.endsWith('write')) {
			setBackends(cid, { evt_upload_speed: speed });
		}
	});

	// Connection user status
	rdb.watch('/system/backends/*/user_status/text', async (key: string, value: MxGenericType) => {
		const cid = extract_backend_name(key);
		const user_status = value.astype('string');

		const conkey = '/system/backends/' + cid + '/user_status/color';
		const res = await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(conkey)], 'generic');
		const user_color = res.astype('string');
		setBackends(cid, { user_color, user_status });
	});

	MxWebsocket.instance.subscribe('mxrdb::keydeleted', (data: Uint8Array) => {
		const key: string = MxGenericType.fromData(data).astype('string');
		if(key.startsWith('/system/backends/') && key.endsWith('connected')) {
			const cid = extract_backend_name(key);
			delete_backend_metadata(BigInt('0x' + cid));
		}
	});
}

function delete_backend_metadata(cid: BigInt) {
	setBackends(produce(b => { delete b[cid.toString(16)]; }));
}

async function init_global_state() {
	await init_client_status();

	await fetch_client_name();

	await fetch_client_role();

	await fetch_experiment_name();

	await init_backend_status();

	await init_run_status();
}
