import { createStore } from "solid-js/store";
import { MxGenericType } from "./convert";
import { MxWebsocket } from "./websocket";
import { MxRdb } from "./rdb";
import { extract_backend_name } from "./utils";
import { createSignal } from "solid-js";

class BackendStatus {
	name: string;
	host: string;
	connected: boolean;
	evt_upload_speed: number;
	evt_download_speed: number;
	uptime: number;
	user_status: string;
	user_color: string;

	constructor(name: string, host: string, connected: boolean, time: number, ustatus: string, ucolor: string) {
		this.name = name;
		this.host = host;
		this.connected = connected;
		this.evt_upload_speed = 0;
		this.evt_download_speed = 0;
		this.uptime = time;
		this.user_status = ustatus;
		this.user_color = ucolor;
	}
};

interface BackendStatusList {
	[key: string]: BackendStatus;
};

// Global signals/stores
const [backends, setBackends] = createStore<BackendStatusList>();
const [loggedUser, setLoggedUser] = createSignal<string>('');
const [expname, setExpname] = createSignal<string>('');

export {
	// Global state variables
	// This data is always persistent through the entire frontend
	// Would be possible to move all this to a solidjs context
	// and use it on the root file. However I decided to use this
	// and I remember reasoning about it, but now I don't know why...
	backends 	as gBackends,
	loggedUser 	as gLoggedUser,
	expname 	as gExpname,

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

	return new BackendStatus(cname, chost, cconn, Number(ctime), ustatus, ucolor);
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

async function fetch_experiment_name() {
	const name = await MxWebsocket.instance.rpc_call('mulex::SysGetExperimentName');
	setExpname(name.astype('string'));
}

async function init_backend_status() {
	const rdb = new MxRdb();

	// Connection status
	rdb.watch('/system/backends/*/connected', (key: string, value: MxGenericType) => {
		const cid = extract_backend_name(key);
		let prev = { ...backends[cid] };
		prev.connected = value.astype('bool');
		prev.user_status = 'None';

		const conkey = '/system/backends/' + cid + '/last_connect_time';
		MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(conkey)], 'generic').then((res: MxGenericType) => {
			prev.uptime = Number(res.astype('int64'));
			setBackends(cid, () => prev);
		});
	});

	// Backend creation
	rdb.watch('/system/backends/*/name', (key: string, value: MxGenericType) => {
		const cid = extract_backend_name(key);
		let prev = { ...backends[cid] };
		prev.name = value.astype('string');

		const hostkey = '/system/backends/' + cid + '/host';
		MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(hostkey)], 'generic').then((res: MxGenericType) => {
			prev.host = res.astype('string');
			setBackends(cid, () => prev);
		});
	});

	// Connection metrics
	rdb.watch('/system/backends/*/statistics/event/*', (key: string) => {
		MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(key)], 'generic').then((res: MxGenericType) => {
			if(key.endsWith('read')) {
				const cid = extract_backend_name(key);
				let prev = { ...backends[cid] };
				prev.evt_download_speed = res.astype('uint32');
				setBackends(cid, () => prev);
			}
			else if(key.endsWith('write')) {
				const cid = extract_backend_name(key);
				let prev = { ...backends[cid] };
				prev.evt_upload_speed = res.astype('uint32');
				setBackends(cid, () => prev);
			}
		});
	});

	// Connection user status
	rdb.watch('/system/backends/*/user_status/text', (key: string, value: MxGenericType) => {
		const cid = extract_backend_name(key);
		let prev = { ...backends[cid] };
		prev.user_status = value.astype('string');

		const conkey = '/system/backends/' + cid + '/user_status/color';
		MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(conkey)], 'generic').then((res: MxGenericType) => {
			prev.user_color = res.astype('string');
			setBackends(cid, () => prev);
		});
	});
}

async function init_global_state() {
	await init_client_status();

	await fetch_client_name();

	await fetch_experiment_name();

	await init_backend_status();
}
