import { Component, createSignal, createEffect, on } from 'solid-js';
import Sidebar from './components/Sidebar'
import Card from './components/Card';
import BackendStatusTable from './components/BackendStatusTable';
import { MxWebsocket } from './lib/websocket';
import { showToast } from './components/ui/toast';
import { MxRdbTree } from './lib/rdbtree';
import { MxGenericType } from './lib/convert';
import { Button } from './components/Button';
import { MxRdb } from './lib/rdb';
import { BadgeLabel } from './components/ui/badge-label';
import { timestamp_tohms } from './lib/utils';
import { LogTable } from './components/LogTable';
import { ResourcePanel } from './components/ResourcePanel';
import { ClientsTable } from './components/ClientsTable';

const [socketStatus, setSocketStatus] = createSignal<boolean>(false);
const [runStatus, setRunStatus] = createSignal<string>('Stopped');
const [runNumber, setRunNumber] = createSignal<number>(0);
const [runTimestamp, setRunTimestamp] = createSignal<number>(0);

const Home: Component = () => {
	const rdb = new MxRdb();
	const [time, setTime] = createSignal(Date.now() as number);
	setInterval(() => setTime(Date.now() as number), 1000);

	function setRunStatusFromCode(code: number) {
		switch(code) {
			case 0: setRunStatus('Stopped'); break;
			case 1: setRunStatus('Running'); break;
			case 2: setRunStatus('Starting'); break;
			case 3: setRunStatus('Stopping'); break;
		}
	}

	MxWebsocket.instance.on_connection_change((conn: boolean) => {
		setSocketStatus(conn);

		if(conn) {
			MxWebsocket.instance.rpc_call('mulex::RdbListKeys', [], 'generic').then((res) => {
				const keys = res.astype('stringarray');
				const tree = new MxRdbTree(keys);
				tree.print_tree();
			});

			MxWebsocket.instance.rpc_call(
				'mulex::RdbReadValueDirect',
			    [MxGenericType.str512('/system/run/status')],
			    'generic').then((res: MxGenericType) => {
				    const rstatus = res.astype('uint8');
				    // Status MEMO:
				    // 0 - Stopped
				    // 1 - Running
				    // 2 - Starting
				  	// 3 - Stopping
					setRunStatusFromCode(rstatus);
			  	});

			MxWebsocket.instance.rpc_call(
				'mulex::RdbReadValueDirect',
			    [MxGenericType.str512('/system/run/number')],
			    'generic').then((res: MxGenericType) => {
				    const rnumber = res.astype('uint64');
					setRunNumber(Number(rnumber));
			  	});

			MxWebsocket.instance.rpc_call(
				'mulex::RdbReadValueDirect',
			    [MxGenericType.str512('/system/run/timestamp')],
			    'generic').then((res: MxGenericType) => {
				    const rts = res.astype('int64');
					setRunTimestamp(Number(rts));
			  	});
		}
	});

	rdb.watch('/system/run/status', (_: string, value: MxGenericType) => {
		setRunStatusFromCode(value.astype('uint8'));
		if(value.astype('uint8') == 1) {
			showToast({ title: 'Run ' + runNumber() + ' started.', variant: 'success'});
		}
		else if(value.astype('uint8') == 0) {
			showToast({ title: 'Run ' + runNumber() + ' stopped.', variant: 'error'});
		}
	});
	rdb.watch('/system/run/number', (_: string, value: MxGenericType) => {
		setRunNumber(value.astype('uint64'));
	});
	rdb.watch('/system/run/timestamp', (_: string, value: MxGenericType) => {
		setRunTimestamp(Number(value.astype('int64')));
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
						<div class="grid grid-cols-4 grid-rows-4 gap-1">
							<Button
								class="col-span-2 row-span-2 m-1"
								onClick={() => {
									MxWebsocket.instance.rpc_call('mulex::RunStart').then((res: MxGenericType) => {
										const run_ok = res.astype('bool');
										if(run_ok) {
											// TODO: (Cesar)
										}
									});
								}}
								disabled={runStatus() == 'Running'}
							>Start Run</Button>
							<Button
								class="col-span-1 row-span-2 row-start-3 m-1"
								onClick={() => {
									MxWebsocket.instance.rpc_call('mulex::RunStop', [], 'none');
								}}
								disabled={runStatus() != 'Running'}
							>Stop Run</Button>
							<Button
								class="col-span-1 row-span-2 row-start-3 m-1"
								onClick={() => {
									MxWebsocket.instance.rpc_call('mulex::RunReset', [], 'none');
								}}
								disabled={runStatus() != 'Stopped'}
								type="error"
							>Reset Run</Button>
							<div class="col-span-2 mx-5 grid grid-cols-2 gap-5 text-right">
								<span>Server</span>
								<BadgeLabel type={socketStatus() ? "success" : "error"}>{socketStatus() ? 'Connected' : 'Disconnected'}</BadgeLabel>
							</div>
							<div class="col-span-2 mx-5 grid grid-cols-2 gap-5 text-right">
								<span>Run</span>
								<BadgeLabel type={runStatus() == 'Running' ? "success" : "error"}>{runStatus()}</BadgeLabel>
							</div>
							<div class="col-span-2 mx-5 grid grid-cols-2 gap-5 text-right">
								<span>Run No.</span>
								<BadgeLabel type="display">{runNumber().toString()}</BadgeLabel>
							</div>
							<div class="col-span-2 mx-5 grid grid-cols-2 gap-5 text-right">
								<span>Run Time</span>
								<BadgeLabel type="display">{runStatus() == 'Running' ? timestamp_tohms(time() - runTimestamp()) : '0s'}</BadgeLabel>
							</div>
						</div>
					</Card>
					<Card title="Backends">
						<BackendStatusTable/>
					</Card>
					<Card title="Server Resources">
						<ResourcePanel/>
					</Card>
					<Card title="Logs">
						<LogTable/>
					</Card>
					<Card title="Clients">
						<ClientsTable/>
					</Card>
				</div>
			</div>
		</div>
	);
};

export default Home;
