import { Component, createEffect, createSignal, For, on, onMount } from "solid-js";
import { DynamicTitle } from "./components/DynamicTitle";
import Sidebar from "./components/Sidebar";
import Card from "./components/Card";
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "./components/ui/table";
import { MxWebsocket } from "./lib/websocket";
import { MxGenericType } from "./lib/convert";
import { createStore } from "solid-js/store";
import { BadgeLabel } from "./components/ui/badge-label";
import { MxRdb } from "./lib/rdb";
import { Dialog, DialogContentWide, DialogDescription, DialogHeader, DialogTitle, DialogTrigger } from "./components/ui/dialog";
import { concat_bytes, download_data } from "./lib/utils";
import { MxPopup } from "./components/Popup";
import { MxButton, MxSpinner, MxSwitch } from "./api";
import { untrack } from "solid-js/web";

interface RunEntry {
	id: number;
	alias: string;
	start_ts: string;
	stop_ts: string;
	status: string;
};

interface RunEntryArray {
	items: Array<RunEntry>;
};

interface RunLogFile {
	handle: string;
	alias: string;
	timestamp: string;
};

interface RunLogFileStore {
	[key: string]: Array<RunLogFile>;
};

const RunLogsFilesTable : Component<{ run: number, onlyLastVersion: boolean, refresh: number }> = (props) => {
	let logFileCache: RunLogFileStore;
	const [logFiles, logFilesActions] = createStore<RunLogFileStore>();
	const [filename, setFilename] = createSignal<string>('');
	const rdb = new MxRdb();

	function changeDisplayMode(onlyLastVersion: boolean) {
		if(onlyLastVersion) {
			for(const [bckName, logfiles] of Object.entries(logFiles)) {
				const existMap = new Map<string, RunLogFile>();

				for(const file of logfiles) {
					const existing = existMap.get(file.alias);
					if(!existing || file.timestamp > existing.timestamp) {
						existMap.set(file.alias, file);
					}
				}

				logFilesActions(bckName, () => Array.from(existMap.values()));
			}
		}
		else {
			logFilesActions(logFileCache);
		}
	}

	async function setupTable() {

		for(const key of Object.keys(logFiles)) {
			logFilesActions(key, []);
		}

		const res = await MxWebsocket.instance.rpc_call('mulex::RunLogGetMeta', [MxGenericType.uint64(BigInt(props.run))], 'generic');
		const cache = new Map<BigInt, string>();
		if(!res.isEmpty()) {
			const files = res.unpack(['str512', 'str512', 'uint64', 'str512']);
			for(const file of files) {
				const [handle, alias, cid, timestamp] = file;

				if(!cache.has(cid)) {
					const bckName = await rdb.read('/system/backends/' + cid.toString(16) + '/name');
					cache.set(cid, bckName);
				}

				logFilesActions(cache.get(cid)!, (p) => p ? [...p, { handle, alias, timestamp }] : [{ handle, alias, timestamp }]);
			}

			logFileCache = { ...logFiles };

			changeDisplayMode(props.onlyLastVersion);
		}
	}

	createEffect(on(() => props.refresh, () => setupTable()));

	createEffect(on(() => props.onlyLastVersion, (value) => {
		changeDisplayMode(value);
	}));

	async function downloadFile(handle: string, name: string) : Promise<boolean> {
		const CHUNK_SIZE = 64 * 1024;
		const res = await MxWebsocket.instance.rpc_call('mulex::FdbChunkedDownloadStart', [
			MxGenericType.str512(handle),
			MxGenericType.int64(BigInt(CHUNK_SIZE))
		]);

		if(res.astype('bool')) {
			setFilename(name);
			const chunks = [];
			while(true) {
				const chunk = await MxWebsocket.instance.rpc_call('mulex::FdbChunkedDownloadReceive', [MxGenericType.str512(handle)], 'generic');
				const [last, buffer] = chunk.unpack(['uint8', 'bytearray'])[0];
				chunks.push(buffer);

				if(last == 1) {
					await MxWebsocket.instance.rpc_call('mulex::FdbChunkedDownloadEnd', [MxGenericType.str512(handle)]);
					console.log(concat_bytes(chunks));
					download_data(name, concat_bytes(chunks), 'application/octet-stream');
					setFilename('');
					return true;
				}
			}
		}
		return false;
	}

	return (
		<div class="w-full">
			<Table>
				<TableHeader>
					<TableRow>
						<TableHead class="w-32">Backend</TableHead>
						<TableHead>File</TableHead>
						<TableHead class="w-64">Timestamp</TableHead>
					</TableRow>
				</TableHeader>
				<TableBody>
					<For each={Object.entries(logFiles)}>{([key, value]) =>
						<>
							<TableRow class="bg-white" onClick={() => {}}>
								<TableCell class="px-5 py-0">{key}</TableCell>
								<TableCell class="px-5 py-0"></TableCell>
								<TableCell class="px-5 py-0"></TableCell>
							</TableRow>
							<For each={value}>{(item: RunLogFile) =>
								<TableRow
									class="even:bg-gray-100 odd:bg-gray-200 cursor-pointer hover:bg-yellow-100"
									onClick={() => downloadFile(item.handle, item.alias)}
								>
									<TableCell class="px-5 py-0"></TableCell>
									<TableCell class="px-5 py-0">{item.alias}</TableCell>
									<TableCell class="px-5 py-0">{item.timestamp}</TableCell>
								</TableRow>
							}</For>
						</>
					}</For>
				</TableBody>
			</Table>
			<MxPopup title="Please wait..." open={filename() !== ''}>
				<MxSpinner description={`Downloading ${filename()}`}/>
			</MxPopup>
		</div>
	);
};

const RunLogsTable : Component = () => {
	const RUNS_PER_PAGE = 50n;
	const [runs, runsActions] = createStore<RunEntryArray>({ items: [] });
	const [page, setPage] = createSignal<number>(1);
	const [onlyLast, setOnlyLast] = createSignal<boolean>(true);
	const [refresh, setRefresh] = createSignal<number>(0);
	const [runToInspect, setRunToInspect] = createSignal<number>(0);
	const [openPopup, setOpenPopup] = createSignal<boolean>(false);
	const rdb = new MxRdb();

	async function updateLogsPage(page: number) {
		runsActions("items", []);
		const res = await MxWebsocket.instance.rpc_call('mulex::RunLogGetRuns', [MxGenericType.uint64(RUNS_PER_PAGE), MxGenericType.uint64(BigInt(page))], 'generic');
		if(!res.isEmpty()) {
			const data = res.unpack(['uint64', 'str512', 'str512', 'str512']);

			for(const run of data) {
				const [ id, alias, start_ts, stop_ts ] = run;
				runsActions("items", (p) => [...p, { id: Number(id), alias, start_ts, stop_ts, status: (stop_ts ? 'Stopped' : 'Running') }]);
			}
		}
	}

	rdb.watch('/system/run/status', () => page() === 1 ? updateLogsPage(0) : null);

	onMount(() => {
		updateLogsPage(0);
	});

	function getRunStatusBadge(status: string) {
		return <BadgeLabel type={status === 'Stopped' ? 'error' : 'success'}>{status}</BadgeLabel>;
	}

	return (
		<>
			<Table>
				<TableHeader>
					<TableRow>
						<TableHead class="w-32">Run</TableHead>
						<TableHead>Alias</TableHead>
						<TableHead class="w-32">Status</TableHead>
						<TableHead class="w-48">Start Timestamp</TableHead>
						<TableHead class="w-48">Stop Timestamp</TableHead>
					</TableRow>
				</TableHeader>
				<TableBody>
					<For each={runs.items}>{(run: RunEntry, index) =>
						<TableRow class="even:bg-gray-200 cursor-pointer hover:bg-yellow-100" onClick={() => { setRunToInspect(index()); setOpenPopup(true); }}>
							<TableCell class="px-5 py-1">#{run.id}</TableCell>
							<TableCell class="px-5 py-1">{run.alias ? run.alias : '---'}</TableCell>
							<TableCell class="px-5 py-1">{getRunStatusBadge(run.status)}</TableCell>
							<TableCell class="px-5 py-1">{run.start_ts}</TableCell>
							<TableCell class="px-5 py-1">{run.stop_ts}</TableCell>
						</TableRow>
					}</For>
				</TableBody>
			</Table>

			<Dialog open={openPopup()} onOpenChange={setOpenPopup}>
				<DialogTrigger/>
				<DialogContentWide>
					<DialogHeader>
						<DialogTitle>
							<div class="flex place-content-between mx-5">
								<span>Run #{runs.items[runToInspect()].id}</span>
								<MxSwitch label="Show only last file version" value={onlyLast()} onChange={setOnlyLast}/>
								<MxButton
									type="success"
									onClick={() => setRefresh(refresh() + 1)}
									disabled={runs.items[runToInspect()].status === 'Stopped'}
								>
									Refresh
								</MxButton>
								{getRunStatusBadge(runs.items[runToInspect()].status)}
							</div>
						</DialogTitle>
						<DialogDescription>
							{runs.items[runToInspect()].alias}
						</DialogDescription>
						<RunLogsFilesTable run={runs.items[runToInspect()].id} onlyLastVersion={onlyLast()} refresh={refresh()}/>
					</DialogHeader>
				</DialogContentWide>
			</Dialog>
		</>
	);
};

export const RunViewer: Component = () => {
	return (
		<div>
			<DynamicTitle title="Run Panel"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				{
				// <Card title="Run Status">
				// </Card>
				}

				<Card title="Run Logs">
					<RunLogsTable/>
				</Card>
			</div>
		</div>
	);
};
