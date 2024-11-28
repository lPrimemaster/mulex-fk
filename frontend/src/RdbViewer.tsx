import { Component, createSignal, useContext } from 'solid-js';
import Sidebar from './components/Sidebar';
import Card from './components/Card';
import SearchBar from './components/SearchBar';
import { MxWebsocket } from './lib/websocket';
import { MxGenericType } from './lib/convert';
import { createSetStore } from './lib/rset';

const [rdbKeys, rdbKeysAction] = createSetStore<string>();
// const [rdbCurrentKey, setRdbCurrentKey] = createSignal<string>();

export const RdbViewer: Component = () => {

	MxWebsocket.instance.on_connection_change((conn: boolean) => {
		if(conn) {
			MxWebsocket.instance.rpc_call('mulex::RdbListKeys', [], 'generic').then((data) => {
				const rdbkeys: Array<string> = data.astype('stringarray');
				rdbKeysAction.set(rdbkeys);
			});
		}
	});

	if(rdbKeys.data.size === 0) {
		MxWebsocket.instance.rpc_call('mulex::RdbListKeys', [], 'generic').then((data) => {
			const rdbkeys: Array<string> = data.astype('stringarray');
			rdbKeysAction.set(rdbkeys);
		});
	}

	MxWebsocket.instance.subscribe('mxrdb::keycreated', (data: Uint8Array) => {
		const key = MxGenericType.fromData(data).astype('string');
		rdbKeysAction.add(key);
	});

	MxWebsocket.instance.subscribe('mxrdb::keydeleted', (data: Uint8Array) => {
		const key = MxGenericType.fromData(data).astype('string');
		rdbKeysAction.remove(key);
	});

	// const rdb = new MxRdb();

	// Connection status
	// rdb.watch('/system/backends/*/connected', (key: string, value: MxGenericType) => {
	// });

	return (
		<div>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<Card title="Rdb Viewer">
					<SearchBar items={Array.from(rdbKeys.data)} placeholder="Search rdb..."/>
				</Card>
			</div>
		</div>
	);
};

