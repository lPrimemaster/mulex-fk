import { Component, createMemo, createSignal, For, onCleanup, onMount, untrack } from "solid-js";
import { DynamicTitle } from "./components/DynamicTitle";
import Sidebar from "./components/Sidebar";
import { MxWebsocket } from "./lib/websocket";
import { createMapStore } from "./lib/rmap";
import { array_chunkify, download_data, timestamp_tohms } from "./lib/utils";
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "./components/ui/table";
import Card from "./components/Card";
import { MxGenericType } from "./lib/convert";
import { MxButton, MxSpinner, MxValueControl } from "./api";
import { MxPopup } from "./components/Popup";
import { MxRdb } from "./lib/rdb";

export const DebugPanel : Component = () => {

	const POLL_F = 2000;
	const [clientAlias, clientAliasActions] = createMapStore<number, string>(new Map<number, string>());
	const [clientCalls, clientCallsActions] = createMapStore<number, number>(new Map<number, number>());
	const [methodCalls, methodCallsActions] = createMapStore<number, number>(new Map<number, number>());
	const [methodTotals, setMethodTotals]   = createSignal<number>(0);
	const [pmethodTotals, setPmethodTotals]   = createSignal<number>(methodTotals());
	const [downloading, setDownloading] = createSignal<string>('');
	const [logCount, setLogCount] = createSignal<number>(1000);
	const methodLoadAvg = createMemo(() => {
		const v = methodTotals();
		const p = untrack(pmethodTotals);
		setPmethodTotals(v);

		return (v - p) / (POLL_F / 1000);
	});
	const [umark, setUmark] = createSignal<number>(0);
	const [time, setTime] = createSignal(Date.now() as number);

	let methodNames: Array<string>;

	onMount(() => {
		MxWebsocket.instance.rpc_call('mulex::RpcGetAllCalls', [], 'generic').then((res: MxGenericType) => {
			const methods = array_chunkify<string>(res.astype('stringarray'), 2);
			methodNames = methods.map(x => x[0]);

			getUptimeMark();
			getRpcCallsDebugInfo();
		});

		const iid = setInterval(() => getRpcCallsDebugInfo(), POLL_F);
		const i2id = setInterval(() => setTime(Date.now() as number), 1000);

		onCleanup(() => {
			clearInterval(iid);
			clearInterval(i2id);
		});
	});

	async function getUptimeMark() {
		const umark = await MxWebsocket.instance.rpc_call('mulex::SysGetUptimeMark', []);
		setUmark(Number(umark.astype('int64')));
	}

	async function getRpcCallsDebugInfo() {
		const data = await MxWebsocket.instance.rpc_call('mulex::RpcGetCallsDebugData', [], 'generic');
		const calls: Array<Array<any>> = data.unpack(['uint64', 'uint16', 'uint64']);

		// Total calls per client
		const clients = calls.map((x) => x[0]).filter((v, i, a) => a.indexOf(v) === i);
		for(const c of clients) {
			if(!clientCalls.data.has(Number(c))) {
				clientCallsActions.add(Number(c), 0);

				const namekey = '/system/backends/' + c.toString(16) + '/name';
				const exists = await MxWebsocket.instance.rpc_call('mulex::RdbValueExists', [MxGenericType.str512(namekey)]);
				if(!exists.astype('bool')) {
					clientAliasActions.add(Number(c), '<ghost>');
				}
				else {
					const alias = await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(namekey)], 'generic');
					clientAliasActions.add(Number(c), alias.astype('string'));
				}
			}
			clientCallsActions.modify(Number(c), Number(calls.filter((x) => x[0] === c).map((x) => x[2]).reduce((s, v) => s + v, 0n)));
		}

		// Total calls per method
		const methods = calls.map((x) => x[1]).filter((v, i, a) => a.indexOf(v) === i);
		methods.forEach((m) => {
			if(!methodCalls.data.has(m)) {
				methodCallsActions.add(m, 0);
			}
			methodCallsActions.modify(m, Number(calls.filter((x) => x[1] === m).map((x) => x[2]).reduce((s, v) => s + v, 0n)));
		});

		setMethodTotals(Array.from(methodCalls.data.values()).reduce((s, v) => s + v, 0));
	}

	async function downloadLastLogs() {
		setDownloading('Downloading...');
		const res: MxGenericType = await MxWebsocket.instance.rpc_call('mulex::MsgGetLastLogs', [MxGenericType.uint32(logCount())], 'generic');
		setDownloading('Converting...');
		const logs = res.unpack(['int32', 'uint8', 'uint64', 'int64', 'str512']);
		const logsString = JSON.stringify(logs.reverse(), (_, v) => typeof v === 'bigint' ? '0x' + v.toString(16) : v);
		download_data('logs.json', logsString, 'application/json');
		setDownloading('');
	}

	async function dumpRdbKeys() {
		setDownloading('Downloading keys...');
		const data: MxGenericType = await MxWebsocket.instance.rpc_call('mulex::RdbListKeys', [], 'generic');
		const rdbkeys: Array<string> = data.astype('stringarray');
		let output = [];

		const rdb = new MxRdb();
		for(const key of rdbkeys) {
			setDownloading('Downloading [' + key + ']...');
			const types: MxGenericType = await MxWebsocket.instance.rpc_call('mulex::RdbReadKeyMetadata', [MxGenericType.str512(key)], 'generic');
			const ktype = MxGenericType.typeFromTypeid(types.astype('uint8'));
			output.push({
				key: key,
				type: ktype,
				value: await rdb.read(key)
			});
		}

		download_data('rdb_keys.json', JSON.stringify(output, (_, v) => typeof v === 'bigint' ? '0x' + v.toString(16) : v), 'application/json');
		setDownloading('');
	}

	return (
		<div>
			<DynamicTitle title="Debug"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<div class="columns-2 gap-2">
					<div class="break-inside-avoid"><Card title="RPC calls per method">
						<Table>
							<TableHeader>
								<TableRow>
									<TableHead>Method ID</TableHead>
									<TableHead>Method Name</TableHead>
									<TableHead>No. Calls</TableHead>
								</TableRow>
							</TableHeader>
							<TableBody>
								<For each={Array.from(methodCalls.data.keys())}>{(mid: number) =>
									<TableRow class="even:bg-gray-200">
										<TableCell class="px-5 py-0">{mid}</TableCell>
										<TableCell class="px-5 py-0">{methodNames[mid]}</TableCell>
										<TableCell class="px-5 py-0">{methodCalls.data.get(mid)!}</TableCell>
									</TableRow>
								}</For>
								<TableRow class="even:bg-gray-200">
									<TableCell class="px-5 py-0 font-bold">Total</TableCell>
									<TableCell class="px-5 py-0 font-bold">---</TableCell>
									<TableCell class="px-5 py-0 font-bold">{methodTotals()}</TableCell>
								</TableRow>
								<TableRow class="even:bg-gray-200">
									<TableCell class="px-5 py-0 font-bold">Total Freq.</TableCell>
									<TableCell class="px-5 py-0 font-bold">---</TableCell>
									<TableCell class="px-5 py-0 font-bold">{methodLoadAvg()}</TableCell>
								</TableRow>
							</TableBody>
						</Table>
					</Card></div>
					<div class="break-inside-avoid"><Card title="Server info">
						<Table>
							<TableHeader>
								<TableRow>
									<TableHead>Property</TableHead>
									<TableHead>Value</TableHead>
								</TableRow>
							</TableHeader>
							<TableBody>
								<TableRow class="even:bg-gray-200">
									<TableCell class="px-5 py-0">Uptime</TableCell>
									<TableCell class="px-5 py-0">{timestamp_tohms(time() - umark())}</TableCell>
								</TableRow>
								<TableRow class="even:bg-gray-200">
									<TableCell class="px-5 py-0">Version</TableCell>
									<TableCell class="px-5 py-0">{__APP_VERSION__} ({__APP_VNAME__})</TableCell>
								</TableRow>
								<TableRow class="even:bg-gray-200">
									<TableCell class="px-5 py-0">Build Stamp</TableCell>
									<TableCell class="px-5 py-0">{__APP_GHASH__}-{__APP_GBRANCH__}</TableCell>
								</TableRow>
							</TableBody>
						</Table>
					</Card></div>
					<div class="break-inside-avoid"><Card title="RPC calls per client">
						<Table>
							<TableHeader>
								<TableRow>
									<TableHead>Client ID</TableHead>
									<TableHead>Alias</TableHead>
									<TableHead>No. Calls</TableHead>
								</TableRow>
							</TableHeader>
							<TableBody>
								<For each={Array.from(clientCalls.data.keys())}>{(cid: number) =>
									<TableRow class="even:bg-gray-200">
										<TableCell class="px-5 py-0">0x{cid.toString(16)}</TableCell>
										<TableCell class="px-5 py-0">{clientAlias.data.get(cid)!}</TableCell>
										<TableCell class="px-5 py-0">{clientCalls.data.get(cid)!}</TableCell>
									</TableRow>
								}</For>
								<TableRow class="even:bg-gray-200">
									<TableCell class="px-5 py-0 font-bold">Total</TableCell>
									<TableCell class="px-5 py-0 font-bold">---</TableCell>
									<TableCell class="px-5 py-0 font-bold">{Array.from(clientCalls.data.values()).reduce((s, v) => s + v, 0)}</TableCell>
								</TableRow>
							</TableBody>
						</Table>
					</Card></div>
					<div class="break-inside-avoid"><Card title="Misc">
						<div class="flex gap-5 m-5">
							<MxButton onClick={downloadLastLogs}>Download logs</MxButton>
							<MxValueControl title="Count" value={logCount()} onChange={setLogCount} size="small" min={1} max={2000} increment={100}/>
						</div>
						<div class="flex gap-5 m-5">
							<MxButton onClick={dumpRdbKeys}>Dump RDB keys</MxButton>
						</div>
						<MxPopup title="Please wait..." open={downloading().length > 0}>
							<MxSpinner description={downloading()}/>
						</MxPopup>
					</Card></div>
				</div>
			</div>
		</div>
	);
};
