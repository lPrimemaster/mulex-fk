import { Component, createSignal, createEffect, on, onMount } from 'solid-js';
import Sidebar from './components/Sidebar'
import Card from './components/Card';
import BackendStatusTable from './components/BackendStatusTable';
import { MxWebsocket } from './lib/websocket';
import { showToast } from './components/ui/toast';
import { MxGenericType } from './lib/convert';
import { MxButton } from './api/Button';
import { BadgeLabel } from './components/ui/badge-label';
import { timestamp_tohms } from './lib/utils';
import { LogTable } from './components/LogTable';
import { ResourcePanel } from './components/ResourcePanel';
import { ClientsTable } from './components/ClientsTable';
import { DynamicTitle } from './components/DynamicTitle';
import { MxPopup } from './components/Popup';
import { gRunNumber, gRunStatus, gRunTimestamp, gSocketStatus } from './lib/globalstate';

const RunConfigure : Component<{ open: boolean, onOpenChange: (o: boolean) => void }> = (props) => {
	const [runAlias, setRunAlias] = createSignal<string>('');
	const [runDescription, setRunDescription] = createSignal<string>('');

	onMount(async () => {
		const res = await MxWebsocket.instance.rpc_call('mulex::RunGetConfiguration', [], 'generic');
		const [alias, desc] = res.unpack(['str512', 'str512'])[0];

		setRunAlias(alias);
		setRunDescription(desc);
	});

	return (
		<MxPopup title="Configure Runs" open={props.open} onOpenChange={props.onOpenChange}>
			<div class="place-items-center">
				<form onSubmit={(e) => { 
					e.preventDefault();
					MxWebsocket.instance.rpc_call('mulex::RunConfigure', [
						MxGenericType.str512(runAlias()),
						MxGenericType.str512(runDescription()),
						MxGenericType.uint64(0n)
					], 'none');
					props.onOpenChange(false);
				}} class="space-y-4 w-full">
					<div>
						<label class="block text-gray-700 text-sm font-medium mb-1">Alias</label>
						<input
							type="text"
							maxlength={512}
							value={runAlias()}
							onInput={(e) => setRunAlias(e.currentTarget.value)}
							disabled={gRunStatus() != 'Stopped'}
							placeholder="Run %n"
							class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
						/>
					</div>
					<div>
						<label class="block text-gray-700 text-sm font-medium mb-1">Description</label>
						<textarea
							maxlength={512}
							value={runDescription()}
							onInput={(e) => setRunDescription(e.currentTarget.value)}
							disabled={gRunStatus() != 'Stopped'}
							placeholder="Run description."
							class="w-full min-h-32 focus:outline-none focus:ring-2 rounded-lg
							overflow-x-hidden resize-none py-2 border border-gray-300 px-4
							focus:ring-blue-500 whitespace-pre-wrap"
						/>
					</div>
					<button
						type="submit"
						class="w-full bg-blue-600 text-white py-2 rounded-lg hover:bg-blue-700 transition"
						disabled={gRunStatus() != 'Stopped'}
					>
						Done
					</button>
				</form>
			</div>
		</MxPopup>
	);
};

const Home: Component = () => {
	const [runReset, setRunReset] = createSignal<boolean>(false);
	const [runConf, setRunConf] = createSignal<boolean>(false);
	const [runResetCheckPhrase, setRunResetCheckPhrase] = createSignal<string>('');

	const [time, setTime] = createSignal(Date.now() as number);
	setInterval(() => setTime(Date.now() as number), 1000);

	// If the socket status changes, emit a message on the toaster
	let skipToastInit = true;
	createEffect(
		on(gSocketStatus, () => {
			if(skipToastInit) {
				skipToastInit = false;
				return;
			}

			if(gSocketStatus()) {
				showToast({ title: "Connected to server.", variant: "success"});
			}
			else {
				showToast({ title: "Disconnected from server.", description: "Automatic reconnect enabled.", variant: "error"});
			}
		})
	);

	return (
		<div>
			<DynamicTitle title="Home"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<div class="columns-1 xl:columns-2 gap-5">
					<Card title="Status">
						<div class="grid grid-cols-4 grid-rows-4 gap-1">
							<MxButton
								class="col-span-1 row-span-2 m-1"
								onClick={() => {
									MxWebsocket.instance.rpc_call('mulex::RunStart');
								}}
								disabled={gRunStatus() == 'Running'}
							>Start Run</MxButton>
							<MxButton
								class="col-span-1 row-span-2 m-1"
								onClick={() => setRunConf(true)}
								disabled={gRunStatus() != 'Stopped'}
							>Configure Run</MxButton>
							<MxButton
								class="col-span-1 row-span-2 row-start-3 m-1"
								onClick={() => {
									MxWebsocket.instance.rpc_call('mulex::RunStop', [], 'none');
								}}
								disabled={gRunStatus() != 'Running'}
							>Stop Run</MxButton>
							<MxButton
								class="col-span-1 row-span-2 row-start-3 m-1"
								onClick={() => setRunReset(true)}
								disabled={gRunStatus() != 'Stopped'}
								type="error"
							>Reset Run</MxButton>
							<div class="col-span-2 mx-5 grid grid-cols-2 gap-5 text-right">
								<span>Server</span>
								<BadgeLabel type={gSocketStatus() ? "success" : "error"}>{gSocketStatus() ? 'Connected' : 'Disconnected'}</BadgeLabel>
							</div>
							<div class="col-span-2 mx-5 grid grid-cols-2 gap-5 text-right">
								<span>Run</span>
								<BadgeLabel type={gRunStatus() == 'Running' ? "success" : "error"}>{gRunStatus()}</BadgeLabel>
							</div>
							<div class="col-span-2 mx-5 grid grid-cols-2 gap-5 text-right">
								<span>Run No.</span>
								<BadgeLabel type="display">{gRunNumber().toString()}</BadgeLabel>
							</div>
							<div class="col-span-2 mx-5 grid grid-cols-2 gap-5 text-right">
								<span>Run Time</span>
								<BadgeLabel type="display">{gRunStatus() == 'Running' ? timestamp_tohms(time() - gRunTimestamp()) : '0s'}</BadgeLabel>
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

					<MxPopup title="Reset Run" open={runReset()} onOpenChange={setRunReset}>
						<div class="place-items-center">
							<div class="place-content-center justify-center font-semibold text-md">
								You are about to reset all of the run data. This will invalidate ALL DATA.
							</div>
							<div class="place-content-center justify-center font-semibold text-md">
								Type 'I understand.' bellow to confirm.
							</div>
							<div class="place-content-center justify-center font-semibold text-lg mt-2 text-red-500">
								This action is irreversible.
							</div>
							<div class="flex flex-col place-intems-center justify center my-5 w-3/4">
								<input
									type="text"
									value={runResetCheckPhrase()}
									onInput={(e) => setRunResetCheckPhrase(e.currentTarget.value)}
									class="w-full h-10 border border-red-300 rounded-lg
										   px-4 mr-2 py-2 focus:outline-none focus:ring-2
										   focus:ring-red-500 text-red-500 placeholder-red-200"
									placeholder="I understand."
									required
								/>
								<MxButton 
									type="error"
									class="w-full h-10 mt-2"
									onClick={() => {
										MxWebsocket.instance.rpc_call('mulex::RunReset', [], 'none');
										setRunReset(false);
										setRunResetCheckPhrase('');
									}}
									disabled={runResetCheckPhrase() != 'I understand.'}
								>
									Reset Run and Invalidate Data
								</MxButton>
							</div>
						</div>
					</MxPopup>
					<RunConfigure open={runConf()} onOpenChange={setRunConf}/>
				</div>
			</div>
		</div>
	);
};

export default Home;
