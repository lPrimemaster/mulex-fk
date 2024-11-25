import { Component, createSignal, createEffect } from 'solid-js';
import Sidebar from './components/Sidebar'
import Card from './components/Card';
import Status from './components/Status';
import BackendStatusTable from './components/BackendStatusTable';
import { MxWebsocket } from './lib/websocket';
import { showToast } from './components/ui/toast';
import { MxRdbTree } from './lib/rdbtree';
import { MxRdb } from './lib/rdb';
import { MxGenericType } from './lib/convert';

const Home: Component = () => {

	const [socketStatus, setSocketStatus] = createSignal(false);

	MxWebsocket.instance.on_connection_change((conn: boolean) => {
		setSocketStatus(conn);

		if(conn) {
			MxWebsocket.instance.rpc_call('mulex::RdbListKeys', [], 'generic').then((res) => {
				const keys = res.astype('stringarray');

				const tree = new MxRdbTree(keys);
				tree.print_tree();
			});

			const rdb = new MxRdb();
			rdb.watch('/system/backends/*/statistics/event/read', (key: string) => {
				MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(key)], 'generic').then((res: MxGenericType) => {
					console.log('Current bytes/s: ', res.astype('uint32'));
				});
			});
		}
	});

	// If the socket status changes, emit a message on the toaster
	createEffect(() => {
		if(socketStatus()) {
			showToast({ title: "Connected to server.", description: "Connected to websocket at: " + location.host, variant: "success"});
		}
		else {
			showToast({ title: "Disconnected from server.", description: "Automatic reconnect enabled.", variant: "error"});
		}
	});

	return (
		<div>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<div class="grid grid-cols-2 gap-5 my-5">
					<Card title="Status">
						<div class="grid grid-cols-1">
							<Status
								label="Server Connection"
								value={socketStatus() ? "Connected" : "Disconnected"}
								type={socketStatus() ? "success" : "error"}/>
						</div>
					</Card>
					<Card title="Backends">
						<BackendStatusTable/>
					</Card>
					<Card title="Dummy"/>
					<Card title="Clients"/>
				</div>
			</div>
		</div>
	);
};

export default Home;
