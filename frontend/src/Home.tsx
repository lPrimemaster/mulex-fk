import { Component, createSignal, createEffect } from 'solid-js';
import Sidebar from './components/Sidebar'
import Card from './components/Card';
import Status from './components/Status';
import { MxWebsocket } from './lib/websocket';
import { showToast } from './components/ui/toast';

const Home: Component = () => {

	const [socketStatus, setSocketStatus] = createSignal(false);

	MxWebsocket.instance.on_connection_change((conn: boolean) => {
		setSocketStatus(conn);
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
					<Card title="Backends"/>
					<Card title="Dummy"/>
					<Card title="Dummy"/>
				</div>
			</div>
		</div>
	);
};

export default Home;
