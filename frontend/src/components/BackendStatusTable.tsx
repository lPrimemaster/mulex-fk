import { Component, For } from 'solid-js';
import { createStore } from 'solid-js/store';
import { MxWebsocket } from '../lib/websocket';
import { MxRdb } from '../lib/rdb';
import { MxRdbTree } from '../lib/rdbtree';
import { MxGenericType } from '../lib/convert';

class BackendStatus {
	name: string;
	host: string;
	connected: boolean;

	constructor(name: string, host: string, connected: boolean) {
		this.name = name;
		this.host = host;
		this.connected = connected;
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
			rdb.watch('/system/backends/*/connected', (key: string) => {
				const cid = extract_backend_name(key);
				create_client_status(cid).then((status: BackendStatus) => {
					setBackends(cid, () => status);
				});
			});
		}
	});

	return (
		<div>
			<ul>
				<For each={Object.keys(backends)}>{(clientid: string) =>
					<li>{clientid}</li>
				}</For>
			</ul>
		</div>
	);
};

export default BackendStatusTable;
