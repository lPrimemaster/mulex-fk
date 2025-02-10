import { Component, For, Show, createSignal, onMount } from 'solid-js';
import { createStore } from 'solid-js/store';
import { MxWebsocket } from '../lib/websocket';
import { MxRdb } from '../lib/rdb';
import { MxGenericType } from '../lib/convert';
import { BadgeLabel } from './ui/badge-label';
import { BadgeDelta } from './ui/badge-delta';
import { timestamp_tohms } from '../lib/utils';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from './ui/table';

class BackendStatus {
	name: string;
	host: string;
	connected: boolean;
	evt_upload_speed: number;
	evt_download_speed: number;
	uptime: number;

	constructor(name: string, host: string, connected: boolean, time: number) {
		this.name = name;
		this.host = host;
		this.connected = connected;
		this.evt_upload_speed = 0;
		this.evt_download_speed = 0;
		this.uptime = time;
	}
};

interface BackendStatusList {
	[key: string]: BackendStatus;
};

const [backends, setBackends] = createStore<BackendStatusList>();

const BackendStatusTable: Component = () => {

	const [time, setTime] = createSignal(Date.now() as number);

	setInterval(() => setTime(Date.now() as number), 1000);

	async function create_client_status(clientid: string): Promise<BackendStatus> {
		let entry = MxGenericType.str512(`/system/backends/${clientid}/name`);
		const cname: string = (await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [entry], 'generic')).astype('string');
		entry = MxGenericType.str512(`/system/backends/${clientid}/host`);
		const chost: string = (await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [entry], 'generic')).astype('string');
		entry = MxGenericType.str512(`/system/backends/${clientid}/connected`);
		const cconn: boolean = (await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [entry], 'generic')).astype('bool');
		entry = MxGenericType.str512(`/system/backends/${clientid}/last_connect_time`);
		const ctime: number = (await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [entry], 'generic')).astype('int64');

		return new BackendStatus(cname, chost, cconn, Number(ctime));
	}

	function extract_backend_name(key: string): string {
		return key.replace('/system/backends/', '').split('/').shift() as string;
	}

	// MxWebsocket.instance.on_connection_change((conn: boolean) => {
	// 	if(conn) {
	onMount(() => {
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
	});
	// 	}
	// });

	const rdb = new MxRdb();

	// Connection status
	rdb.watch('/system/backends/*/connected', (key: string, value: MxGenericType) => {
		const cid = extract_backend_name(key);
		let prev = { ...backends[cid] };
		prev.connected = value.astype('bool');

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

	function bps_get_multiplier(value: number): number {
		if(Math.trunc(value / 1024) > 0) {
			if(Math.trunc(value / 1048576) > 0) {
				if(Math.trunc(value / 1073741824) > 0) {
					return 3;
				}
				return 2;
			}
			return 1;
		}
		return 0;
	}

	function bps_get_suffix(multiplier: number, bits: boolean): string {
		if(multiplier == 3) return bits ? 'Gbps' : 'GB/s';
		if(multiplier == 2) return bits ? 'Mbps' : 'MB/s';
		if(multiplier == 1) return bits ? 'kbps' : 'kB/s';
		if(multiplier == 0) return bits ?  'bps' :  'B/s';
		return '';
	}

	function bps_to_string(value: number, bits: boolean = true) {
		if(bits) {
			value *= 8;
		}

		const multiplier = bps_get_multiplier(value);
		const suffix = bps_get_suffix(multiplier, bits);
		const str = (value / (1024 ** multiplier)).toFixed(1);

		return str + ' ' + suffix;
	}

	return (
		<div>
			<Table>
				<TableHeader>
					<TableRow>
						<TableHead>Name</TableHead>
						<TableHead>Status</TableHead>
						<TableHead>Host</TableHead>
						<TableHead>Data</TableHead>
						<TableHead>Uptime</TableHead>
					</TableRow>
				</TableHeader>
				<TableBody>
					<For each={Object.keys(backends)}>{(clientid: string) =>
						<TableRow>
							<TableCell class="p-0">{backends[clientid].name}</TableCell>
								<TableCell class="p-0">
								<Show when={backends[clientid].connected}>
									<BadgeLabel type="success">Connected</BadgeLabel>
								</Show>
								<Show when={!backends[clientid].connected}>
									<BadgeLabel type="error">Disconnected</BadgeLabel>
								</Show>
								</TableCell>
							<TableCell class="p-0">
								{backends[clientid].host}
							</TableCell>
							<TableCell class="p-0 flex">
								<span class="p-0 w-28"><BadgeDelta deltaType="increase">{bps_to_string(backends[clientid].evt_upload_speed, false)}</BadgeDelta></span>
								<span class="p-0 w-28"><BadgeDelta deltaType="decrease">{bps_to_string(backends[clientid].evt_download_speed, false)}</BadgeDelta></span>
							</TableCell>
							<TableCell class="p-0">
								<Show when={backends[clientid].connected}>
									{timestamp_tohms(time() - backends[clientid].uptime)}
								</Show>
								<Show when={!backends[clientid].connected}>
									<span class="text-gray-500">Disconnected</span>
								</Show>
							</TableCell>
						</TableRow>
					}</For>
				</TableBody>
			</Table>
		</div>
	);
};

export default BackendStatusTable;
