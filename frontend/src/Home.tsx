import { Component, createSignal, createEffect, on } from 'solid-js';
import Sidebar from './components/Sidebar'
import Card from './components/Card';
import Status from './components/Status';
import BackendStatusTable from './components/BackendStatusTable';
import { MxWebsocket } from './lib/websocket';
import { showToast } from './components/ui/toast';
import { MxRdbTree } from './lib/rdbtree';
import { MxRdb } from './lib/rdb';
import { MxGenericType } from './lib/convert';

const [socketStatus, setSocketStatus] = createSignal(false);

const Home: Component = () => {
	MxWebsocket.instance.on_connection_change((conn: boolean) => {
		setSocketStatus(conn);

		if(conn) {
			MxWebsocket.instance.rpc_call('mulex::RdbListKeys', [], 'generic').then((res) => {
				const keys = res.astype('stringarray');
				const tree = new MxRdbTree(keys);
				tree.print_tree();
			});
		}
	});

	// If the socket status changes, emit a message on the toaster
	let skipToastInit = true;
	createEffect(
		on(socketStatus, () => {
			if(skipToastInit) {
				skipToastInit = false;
				return;
			}

			if(socketStatus()) {
				showToast({ title: "Connected to server.", variant: "success"});
			}
			else {
				showToast({ title: "Disconnected from server.", description: "Automatic reconnect enabled.", variant: "error"});
			}
		})
	);

	return (
		<div>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<div class="columns-1 xl:columns-2 gap-5">
					<Card title="Status">
						<Status
							label="Server Connection"
							value={socketStatus() ? "Connected" : "Disconnected"}
							type={socketStatus() ? "success" : "error"}/>
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
