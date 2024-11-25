import { Component, For } from 'solid-js';
import { createStore } from 'solid-js/store';
import { MxWebsocket } from '../lib/websocket';
import { MxRdb } from '../lib/rdb';
import { MxGenericType } from '../lib/convert';

class BackendStatus {
	name: string;
	host: string;
	connected: boolean;
	evt_upload_speed: number;
	evt_download_speed: number;

	constructor(name: string, host: string, connected: boolean) {
		this.name = name;
		this.host = host;
		this.connected = connected;
		this.evt_upload_speed = 0;
		this.evt_download_speed = 0;
	}
};

interface BackendStatusList {
	[key: string]: BackendStatus;
};

const BackendStatusTable: Component = () => {

	const [backends, setBackends] = createStore<BackendStatusList>();

	async function create_client_status(clientid: string): Promise<BackendStatus> {
		let entry = MxGenericType.str512(`/system/backends/${clientid}/name`);
		const cname: string = (await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [entry], 'generic')).astype('string');
		entry = MxGenericType.str512(`/system/backends/${clientid}/host`);
		const chost: string = (await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [entry], 'generic')).astype('string');
		entry = MxGenericType.str512(`/system/backends/${clientid}/connected`);
		const cconn: boolean = (await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [entry], 'generic')).astype('bool');

		return new BackendStatus(cname, chost, cconn);
	}

	function extract_backend_name(key: string): string {
		return key.replace('/system/backends/', '').split('/').shift() as string;
	}

	MxWebsocket.instance.on_connection_change((conn: boolean) => {
		if(conn) {
			MxWebsocket.instance.rpc_call('mulex::RdbListSubkeys', [MxGenericType.str512('/system/backends/*/name')], 'generic').then((data) => {
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
			});

			const rdb = new MxRdb();

			// Connection status
			rdb.watch('/system/backends/*/connected', (key: string, value: MxGenericType) => {
				const cid = extract_backend_name(key);
				let prev = { ...backends[cid] };
				prev.connected = value.astype('bool');
				setBackends(cid, () => prev);
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
		}
	});

	return (
		<div>
			<ul>
				<For each={Object.keys(backends)}>{(clientid: string) =>
					<li>{clientid} - {backends[clientid].name} - {backends[clientid].connected ? 'Connected' : 'Disconnected'} - Up: {backends[clientid].evt_upload_speed*8} bps - Down: {backends[clientid].evt_download_speed*8} bps</li>
				}</For>
			</ul>
		</div>
	);
};

export default BackendStatusTable;
