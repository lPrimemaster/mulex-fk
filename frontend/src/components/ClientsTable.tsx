import { Component, For } from 'solid-js';
import { MxWebsocket } from '~/lib/websocket';
import { MxGenericType } from '~/lib/convert';
import { createMapStore } from '~/lib/rmap';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from './ui/table';
import { timestamp_tolocaldatetime } from '~/lib/utils';

interface ClientInfo {
	ip: string;
	timestamp: number;
	id: number;
};

const [clients, clientsActions] = createMapStore(new Map<number, ClientInfo>());

export const ClientsTable : Component = () => {

	function watchClients() {
		MxWebsocket.instance.subscribe('mxhttp::newclient', (data: Uint8Array) => {
			const client = MxGenericType.fromData(data).unpack(['str32', 'int64', 'uint64'])[0];
			clientsActions.add(client[2], { ip: client[0], timestamp: client[1], id: client[2] });
		});

		MxWebsocket.instance.subscribe('mxhttp::delclient', (data: Uint8Array) => {
			clientsActions.remove(MxGenericType.fromData(data).astype('uint64'));
		});
	}

	watchClients();

	MxWebsocket.instance.on_connection_change((conn: boolean) => {
		if(conn) {
			MxWebsocket.instance.rpc_call('mulex::HttpGetClients', [], 'generic').then((data: MxGenericType) => {
				const client_list = data.unpack(['str32', 'int64', 'uint64']);

				clientsActions.set(new Map<number, ClientInfo>());

				for(const client of client_list) {
					clientsActions.add(client[2], { ip: client[0], timestamp: client[1], id: client[2] });
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
						<TableHead class="text-center">Identifier</TableHead>
						<TableHead class="text-center">First seen</TableHead>
					</TableRow>
				</TableHeader>
				<TableBody>
					<For each={Array.from(clients.data.entries())}>{(client) =>
						<TableRow>
							<TableCell class="p-0 text-center">{client[1].ip}</TableCell>
							<TableCell class="p-0 text-center">{Number(client[1].id)}</TableCell>
							<TableCell class="p-0 text-center">{timestamp_tolocaldatetime(client[1].timestamp)}</TableCell>
						</TableRow>
					}</For>
				</TableBody>
			</Table>
		</div>
	);
};
