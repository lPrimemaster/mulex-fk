import { Component, For } from 'solid-js';
import { MxWebsocket } from '~/lib/websocket';
import { MxGenericType } from '~/lib/convert';
import { createMapStore } from '~/lib/rmap';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from './ui/table';
import { timestamp_tolocaldatetime } from '~/lib/utils';

interface ClientInfo {
	ip: string;
	timestamp: number;
	connections: Set<number>;
};

const [clients, clientsActions] = createMapStore(new Map<string, ClientInfo>());

export const ClientsTable : Component = () => {

	function addClient(client: Array<any>) {
			if(clients.data.has(client[0])) {
				const c = clients.data.get(client[0])!;
				const connections = c.connections;
				connections.add(client[2]);
				clientsActions.modify(client[0], { ip: client[0], timestamp: c.timestamp, connections: connections });
			}
			else {
				clientsActions.add(client[0], { ip: client[0], timestamp: client[1], connections: new Set<number>([client[2]]) });
			}
	}

	function removeClient(id: number) {
			const keys = [...clients.data.entries()].filter(({ 1: v }) => v.connections.has(id)).map(([k]) => k);

			if(keys.length === 1) {
				const client = clients.data.get(keys[0])!;
				if(client.connections.size > 1) {
					const connections = client.connections;
					connections.delete(id);
					clientsActions.modify(keys[0], { ip: client.ip, timestamp: client.timestamp, connections: connections });
				}
				else {
					clientsActions.remove(keys[0]);
				}
			}
			else {
				console.error('Expected 1 key only. [mxhttp::delclient]');
			}
	}

	function watchClients() {
		MxWebsocket.instance.subscribe('mxhttp::newclient', (data: Uint8Array) => {
			const client = MxGenericType.fromData(data).unpack(['str32', 'int64', 'uint64'])[0];
			addClient(client);
		});

		MxWebsocket.instance.subscribe('mxhttp::delclient', (data: Uint8Array) => {
			const id = MxGenericType.fromData(data).astype('uint64');
			removeClient(id);
		});
	}

	watchClients();

	MxWebsocket.instance.on_connection_change((conn: boolean) => {
		if(conn) {
			MxWebsocket.instance.rpc_call('mulex::HttpGetClients', [], 'generic').then((data: MxGenericType) => {
				const client_list = data.unpack(['str32', 'int64', 'uint64']);

				clientsActions.set(new Map<string, ClientInfo>());
				for(const client of client_list) {
					addClient(client);
				}
			});

			watchClients();
		}
	});

	return (
		<div>
			<Table>
				<TableHeader>
					<TableRow>
						<TableHead class="text-center">IP</TableHead>
						<TableHead class="text-center">Active Connections</TableHead>
						<TableHead class="text-center">First seen</TableHead>
					</TableRow>
				</TableHeader>
				<TableBody>
					<For each={Array.from(clients.data.entries())}>{(client) =>
						<TableRow>
							<TableCell class="p-0 text-center">{client[1].ip}</TableCell>
							<TableCell class="p-0 text-center">{Number(client[1].connections.size)}</TableCell>
							<TableCell class="p-0 text-center">{timestamp_tolocaldatetime(client[1].timestamp)}</TableCell>
						</TableRow>
					}</For>
				</TableBody>
			</Table>
		</div>
	);
};
