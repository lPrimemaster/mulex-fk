import { Component, createEffect, createSignal, Show, on, For } from 'solid-js';
import { createStore, produce } from 'solid-js/store';
import Sidebar from './components/Sidebar';
import { DynamicTitle } from './components/DynamicTitle';
import { DiGraph } from './components/DiGraph';
import { MxWebsocket } from './lib/websocket';
import Card from './components/Card';
import { MxDoubleSwitch } from './api/Switch';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from './components/ui/table';
import { MxSpinner } from './api';
import { bps_to_string, bytes_to_string, event_io_extract } from './lib/utils';
import { createMapStore } from './lib/rmap';
import { BadgeDelta } from './components/ui/badge-delta';
import { BadgeLabel } from './components/ui/badge-label';
import { MxInlineGraph } from './components/InlineGraph';
import { MxPopup } from './components/Popup';
import { MxCaptureBadge, MxTickBadge } from './components/Badges';
import { createSetStore } from './lib/rset';
import { MxTree, MxTreeNode } from './components/TreeNode';
import { MxHexTable } from './components/HexTable';

interface EventIO {
	read: number;
	write: number;

	lread?: Array<number>;
	lwrite?: Array<number>;
};

interface ClientsIO {
	[key: string]: EventIO;
};

interface EventMeta {
	name: string;
	trigger: number; // In number of frames/cycles
	issys: boolean;
	capture: boolean;

	// IO
	io: EventIO;
	clients?: ClientsIO;
};

interface CaptureMeta {
	totalbytes: number;
	autoCapture: boolean;
}

interface EventMetaList {
	[key: number]: EventMeta;
};

interface CaptureMetaList {
	[key: number]: CaptureMeta;
};

interface CaptureDataList {
	[key: number]: Uint8Array;
};

export const EventsViewer : Component = () => {

	const [gmode, setGmode] = createSignal<boolean>(false);
	const [sysEvents, setSysEvents] = createSignal<boolean>(false);
	const [pollFast, setPollFast] = createSignal<boolean>(true);
	const [popupID, setPopupID] = createSignal<number>(0);

	const [eventsMeta, setEventsMeta] = createStore<EventMetaList>();
	const [captureData, setCaptureData] = createStore<CaptureDataList>();
	const [captureMeta, setCaptureMeta] = createStore<CaptureMetaList>();

	const [captureSet, captureSetActions] = createSetStore<number>([]);
	const [captureTick, captureTickActions] = createSetStore<number>([]);
	const [captureCollapse, captureCollapseActions] = createMapStore<number, boolean>(new Map<number, boolean>());


	let iid: NodeJS.Timeout;

	createEffect(on(pollFast, () => {
		clearInterval(iid);
		iid = setInterval(readEventStatistics, pollFast() ? 1000 : 5000);
	}));

	// TODO: (Cesar) This way of detecting system events is poor.
	// 				 What if the user decides to name an event starting with the mx prefix also?
	function checkSystemEvent(name: string) : boolean {
		return name.startsWith('mx') || name.endsWith('::rpc') || name.endsWith('::rpc_res');
	}

	function computeClientsIO(clients: Uint8Array, frames: Uint8Array) : ClientsIO | undefined {
		if(clients.length === 0 || frames.length === 0) {
			return undefined;
		}

		if(clients.length !== frames.length) {
			console.error('Failed to compute clients IO from metadata.');
			return undefined;
		}

		const cio: ClientsIO = {};
		const cview = new DataView(clients.buffer, clients.byteOffset, clients.byteLength);
		const fview = new DataView(frames.buffer, frames.byteOffset, frames.byteLength);

		for(let i = 0; i < clients.byteLength; i += 8) {
			const coffset = i;// + clients.byteOffset;
			const foffset = i;// + frames.byteOffset;
			const { r, w } = event_io_extract(fview.getBigUint64(foffset, true));
			const key = cview.getBigUint64(coffset, true).toString(16);
			cio[key] = { read: r, write: w };
		}

		return cio;
	}
	
	function getSelectedEventName() : string {
		const evt = eventsMeta[popupID()];
		return evt !== undefined ? evt.name : '';
	}

	function getSelectedEventRegistrar() : string {
		const evt = eventsMeta[popupID()];
		return evt !== undefined ? (evt.issys ? 'System' : 'User') : '';
	}

	function getSelectedEventTotalReadString() : string {
		const evt = eventsMeta[popupID()];
		return evt !== undefined ? bps_to_string(evt.io.read, false) : '';
	}

	function getSelectedEventTotalWriteString() : string {
		const evt = eventsMeta[popupID()];
		return evt !== undefined ? bps_to_string(evt.io.write, false) : '';
	}

	function getSelectedEventClientFrames() : Array<[string, EventIO]> {
		const evt = eventsMeta[popupID()];
		return (evt !== undefined && evt.clients !== undefined) ? Array.from(Object.entries(evt.clients)) : [];
	}

	async function readEventStatistics() {
		const data = await MxWebsocket.instance.rpc_call('mulex::EvtGetAllMetadata', [], 'generic');
		const events = data.unpack(['int16', 'str32', 'uint64', 'bytearray', 'bytearray']);

		for(const event of events) {
			const [ eid, name, io, clients, frames ] = event;

			const { r, w } = event_io_extract(io);
			const clientFrames = computeClientsIO(clients, frames);

			if(!(eid in eventsMeta)) {
				setEventsMeta(eid, {
						name: name,
						trigger: 0,
						// NOTE: (Cesar) For now we check for the event being system on the clientside
						issys: checkSystemEvent(name),
						capture: false,

						clients: clientFrames,
						io: {
							read: r,
							write: w,
							lread: [],
							lwrite: []
						}
					});
				continue;
			}

			if(r == 0 && w == 0) {
				setEventsMeta(eid, { trigger: eventsMeta[eid].trigger + 1 });
				continue;
			}

			setEventsMeta(eid, (p) => {
				return {
					trigger: 0,
					clients: clientFrames,
					io: {
						read: r,
						write: w,
						lread: [...(p.io.lread ?? []), r].slice(-20),
						lwrite: [...(p.io.lwrite ?? []), w].slice(-20)
					}
				};
			});
		}
	}

	function modifyCapture(value: boolean) {
		const eid : number = popupID();
		if(eid > 0) {
			setEventsMeta(eid, {
				capture: value
			});

			if(value) {
				captureCollapseActions.add(eid, false);
				captureSetActions.add(eid);
				setCaptureMeta(eid, { totalbytes: 0, autoCapture: true });
				setCaptureData(eid, new Uint8Array());
				MxWebsocket.instance.subscribe(eventsMeta[eid].name, (data: Uint8Array) => {
					captureTickActions.add(eid);

					if(captureMeta[eid].autoCapture) {
						setCaptureData(eid, data);
						setCaptureMeta(eid, (p) => { return { totalbytes: p.totalbytes + data.length }; });
					}
				});
			}
			else {
				captureSetActions.remove(eid);
				// FIX: (Cesar) This could be tricky and unsubscribe us from the event somewhere else!
				MxWebsocket.instance.unsubscribe(eventsMeta[eid].name);
				setCaptureMeta(produce(p => { delete p[eid]; }));
				setCaptureData(produce(p => { delete p[eid]; }));
				captureCollapseActions.remove(eid);
			}
		}
	}

	function eventGotNewFrame(id: number) {
		const new_frame = captureTick.data.has(id);
		if(new_frame) {
			captureTickActions.remove(id);
		}
		return new_frame;
	}

	function objectNumberEntries<T>(obj: { [key: number]: T }) {
		return Object.entries(obj).map(([k, v]) => [Number(k), v] as [number, T]);
	}

	function objectKeys(obj: any) {
		return Object.keys(obj);
	}

	return (
		<div>
			<DynamicTitle title="Events"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto min-h-dvh flex flex-col">
				<Card title="Options">
					<div class="flex gap-10">
						<div class="grid grid-rows-2 grid-cols-2 gap-2 items-center">
							<div class="text-sm font-bold">Mode</div>
							<MxDoubleSwitch labelFalse="Table" labelTrue="Graphical" value={gmode()} onChange={() => setGmode(!gmode())} disabled/>

							<div class="text-sm font-bold">View System Events</div>
							{
								//  HACK: (Cesar) div padding for labelFalse size on options
							}
							<div class="pl-[14px]">
								<MxDoubleSwitch labelFalse="No" labelTrue="Yes" value={sysEvents()} onChange={() => setSysEvents(!sysEvents())}/>
							</div>
						</div>
						<div class="grid grid-rows-2 grid-cols-2 gap-2 items-center">
							<div class="text-sm font-bold">Polling Mode</div>
							<MxDoubleSwitch labelFalse="Slow" labelTrue="Fast" value={pollFast()} onChange={() => setPollFast(!pollFast())}/>
						</div>
					</div>
				</Card>
				<Show when={gmode()}>
					<DiGraph
						nodes={[
							{
								x: 100, y: 100, title: 'Node 1', id: 'n0', content: <div class="text-wrap">Hello content! With wrapping enabled, since it is large.</div>
							},
							{
								x: 300, y: 150, title: 'Node 2', id: 'n1', content: <div>Hello content!</div>
							},
							{
								x: 300, y: 300, title: 'Node 3', id: 'n2', content: <div>Hello content!</div>
							}
						]}
						edges={[{ from: 'n0', to: 'n1', label: '5 Gb/s' }, { from: 'n0', to: 'n2' }]}
					/>
				</Show>
				<Show when={!gmode()}>
					<Card title="Event List">
						<div class="py-0">
							<Show when={objectKeys(eventsMeta).length === 0}>
								<div class="pt-5">
									<MxSpinner description="Waiting for events..."/>
								</div>
							</Show>
							<MxPopup title="Event Details" open={popupID() > 0} onOpenChange={(s: boolean) => { if(!s) setPopupID(0); }}>
								<div class="grid grid-rows-5 grid-cols-2 gap-2">
									<span class="font-bold">Name</span>
									<span>{getSelectedEventName()}</span>

									<span class="font-bold">Registered ID</span>
									<span>{popupID()}</span>

									<span class="font-bold">Registrar</span>
									<span>{getSelectedEventRegistrar()}</span>

									<span class="font-bold">Global I/O</span>
									<span>
										<BadgeDelta class="w-28" deltaType="increase">{getSelectedEventTotalWriteString()}</BadgeDelta>
										<BadgeDelta class="w-28" deltaType="decrease">{getSelectedEventTotalReadString()}</BadgeDelta>
									</span>

									<span class="font-bold">Capture Event Data</span>
									<MxDoubleSwitch
										labelFalse="No"
										labelTrue="Yes"
										value={popupID() > 0 ? eventsMeta[popupID()].capture : false}
										onChange={(value) => modifyCapture(value)}/>
								</div>
								<div class="mt-5">
									<Table>
										<TableHeader>
											<TableRow>
												<TableHead>Client ID</TableHead>
												<TableHead>Client I/O</TableHead>
											</TableRow>
										</TableHeader>
										<TableBody>
											<For each={getSelectedEventClientFrames()}>{(client) =>
												<TableRow>
													<TableCell class="py-1">0x{client[0]}</TableCell>
													<TableCell class="py-1">
														<span>
															<BadgeDelta class="w-28" deltaType="increase">{bps_to_string(client[1].write, false)}</BadgeDelta>
															<BadgeDelta class="w-28" deltaType="decrease">{bps_to_string(client[1].read, false)}</BadgeDelta>
														</span>
													</TableCell>
												</TableRow>
											}</For>
										</TableBody>
									</Table>
									<Show when={getSelectedEventClientFrames().length === 0}>
										<div class="w-full flex items-center justify-items-center">
											<div class="w-full text-center text-gray">No connections</div>
										</div>
									</Show>
								</div>
							</MxPopup>
							<Show when={Object.keys(eventsMeta).length > 0}>
								<Table>
									<TableHeader>
										<TableRow>
											<TableHead class="w-5"></TableHead>
											<TableHead>Name</TableHead>
											<TableHead>ID</TableHead>
											<TableHead>I/O Totals</TableHead>
											<TableHead>Listeners</TableHead>
											<TableHead>Last Trigger</TableHead>
											<TableHead>System</TableHead>
										</TableRow>
									</TableHeader>
									<TableBody>
										{/* TODO: (Cesar) Prevent flicker by not re-rendering the TableRow element */}
										<For each={Array.from(objectNumberEntries(eventsMeta))}>{(evt) =>
											<Show when={sysEvents() || (!sysEvents() && !evt[1].issys)}>
												<TableRow
													class="hover:bg-yellow-100 cursor-pointer even:bg-gray-200"
													onClick={() => setPopupID(evt[0])}
												>
													<TableCell class="py-1 px-1 w-3 gap-1 flex">
														<MxCaptureBadge capture={evt[1].capture}/>
														<MxTickBadge on={evt[1].capture && eventGotNewFrame(evt[0])}/>
													</TableCell>
													<TableCell class="py-1">{evt[1].name}</TableCell>
													<TableCell class="py-1">{evt[0]}</TableCell>
													<TableCell class="py-1 flex items-center">
														<span class="p-0 w-28">
															<BadgeDelta deltaType="increase">{bps_to_string(evt[1].io.write, false)}</BadgeDelta>
														</span>
														<span class="px-2">
															<MxInlineGraph
																color="green"
																height={24}
																width={60}
																class="border bg-success"
																npoints={20}
																values={evt[1].io.lwrite!}
															/>
														</span>
														<span class="p-0 w-28">
															<BadgeDelta deltaType="decrease">{bps_to_string(evt[1].io.read, false)}</BadgeDelta>
														</span>
														<span class="px-2">
															<MxInlineGraph
																color="red"
																height={24}
																width={60}
																class="border bg-error"
																npoints={20}
																values={evt[1].io.lread!}
															/>
														</span>
													</TableCell>
													<TableCell class="py-1">{evt[1].clients === undefined ? 0 : Object.keys(evt[1].clients).length}</TableCell>
													<TableCell class="py-1">{evt[1].trigger > 0 ? `${evt[1].trigger} poll(s) ago` : "Now"}</TableCell>
													<TableCell class="py-1"><BadgeLabel type="display">{evt[1].issys ? "Yes" : "No"}</BadgeLabel></TableCell>
												</TableRow>
											</Show>
										}</For>
									</TableBody>
								</Table>
							</Show>
						</div>
					</Card>
					<Card title="Captures">
						<div class="mt-5">
							<Show when={captureSet.data.size === 0}>
								<div class="w-full flex">
									<div
										class="items-center w-full border-4 border-dashed rounded-md h-20 m-2 place-content-center gap-2 text-gray-400"
									>
										<div class="text-center">No Captures Active.</div>
										<div class="text-center">Capture an event to see its frame here.</div>
									</div>
								</div>
							</Show>
							<Show when={captureSet.data.size !== 0}>
								<MxTree>
									<For each={Object.entries(captureData)}>{([evt, data]) => {
										const key = Number(evt);
										const evtStore: EventMeta = eventsMeta[key];
										const capMeta: CaptureMeta = captureMeta[key];

										return (
											<MxTreeNode
												title={evtStore.name}
												open={!captureCollapse.data.get(key)!}
												onClick={() => captureCollapseActions.modify(key, !captureCollapse.data.get(key)!)}
											>
												<div>
													<div class="w-1/4">
														<div class="grid grid-rows-3 grid-cols-2 gap-2">
															<span class="font-bold">Size</span>
															<span>{captureData[key].length}</span>

															<span class="font-bold">Auto-capture</span>
															<MxDoubleSwitch
																labelFalse="No"
																labelTrue="Yes"
																value={capMeta.autoCapture}
																onChange={(value: boolean) => {
																	setCaptureMeta(key, { autoCapture: value });
																}}
															/>

															<span class="font-bold">Total received</span>
															<span>{bytes_to_string(capMeta.totalbytes)}</span>
														</div>
													</div>
													<div class="w-full mt-5">
														<MxHexTable data={data} bpr={32}/>
													</div>
												</div>
											</MxTreeNode>
										);
									}}</For>
								</MxTree>
							</Show>
						</div>
					</Card>
				</Show>
			</div>
		</div>
	);
};
