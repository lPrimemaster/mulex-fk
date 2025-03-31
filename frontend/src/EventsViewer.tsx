import { Component, createEffect, createSignal, Show, on, onMount, For } from 'solid-js';
import Sidebar from './components/Sidebar';
import { DynamicTitle } from './components/DynamicTitle';
import { DiGraph } from './components/DiGraph';
import { MxWebsocket } from './lib/websocket';
import Card from './components/Card';
import { MxDoubleSwitch } from './api/Switch';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from './components/ui/table';
import { MxSpinner } from './api';
import { bps_to_string, event_io_extract, timestamp_tohms } from './lib/utils';
import { createSetStore } from './lib/rset';
import { createMapStore } from './lib/rmap';
import { BadgeDelta } from './components/ui/badge-delta';

class EventIO {
	read: number;
	write: number;

	constructor(read: number, write: number) {
		this.read = read;
		this.write = write;
	}
};

type ClientsIO = Map<number, EventIO> | undefined;

class EventMeta {
	name: string;
	io: EventIO;
	trigger: number; // In number of frames/cycles
	clients: ClientsIO;
	issys: boolean;

	constructor(name: string, read: number, write: number, trigger: number, issys: boolean, clients: ClientsIO = undefined) {
		this.name = name;
		this.io = new EventIO(read, write);
		this.trigger = trigger;
		this.issys = issys;
		this.clients = clients;
	}
};

export const EventsViewer : Component = () => {

	const [gmode, setGmode] = createSignal<boolean>(false);
	const [sysEvents, setSysEvents] = createSignal<boolean>(false);
	const [pollFast, setPollFast] = createSignal<boolean>(false);
	const [eventMap, eventMapActions] = createMapStore<number, EventMeta>(new Map<number, EventMeta>());


	let iid: NodeJS.Timeout;

	createEffect(on(pollFast, () => {
		clearInterval(iid);
		iid = setInterval(readEventStatistics, pollFast() ? 1000 : 5000);
	}));

	// onMount(() => {
	// 	iid = setInterval(readEventStatistics, pollFast() ? 1000 : 5000);
	// });
	
	// TODO: (Cesar) This way of detecting system events is poor.
	// 				 What if the user decides to name an event starting with the mx prefix also?
	function check_system_event(name: string) : boolean {
		return name.startsWith('mx') || name.endsWith('::rpc') || name.endsWith('::rpc_res');
	}
	

	async function readEventStatistics() {
		const data = await MxWebsocket.instance.rpc_call('mulex::EvtGetAllMetadata', [], 'generic');
		const events = data.unpack(['int16', 'str32', 'uint64', 'bytearray', 'bytearray']);

		for(const event of events) {
			const [ eid, name, io, clients, frames ] = event;

			const { r, w } = event_io_extract(io);

			// NOTE: (Cesar) For now we check for the event being system on the clientside
			if(!eventMap.data.has(eid)) {
				eventMapActions.add(eid, new EventMeta(name, r, w, 0, check_system_event(name), undefined));
				continue;
			}

			const evtStore : EventMeta = eventMap.data.get(eid)!;
			if(r == 0 && w == 0) {
				// No changes on this event
				// skip
				eventMapActions.modify(eid, new EventMeta(name, r, w, evtStore.trigger + 1, evtStore.issys, undefined));
				continue;
			}

			eventMapActions.modify(eid, new EventMeta(name, r, w, 0, evtStore.issys, undefined));
		}
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
							<MxDoubleSwitch labelFalse="Table" labelTrue="Graphical" value={gmode()} onChange={() => setGmode(!gmode())}/>

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
					<Card title="">
						<MxSpinner description="Waiting for events..."/>
						<div class="py-0">
							<Table>
								<TableHeader>
									<TableRow>
										<TableHead>Name</TableHead>
										<TableHead>ID</TableHead>
										<TableHead>I/O Totals</TableHead>
										<TableHead>Listeners</TableHead>
										<TableHead>Last Trigger</TableHead>
										<TableHead>System</TableHead>
									</TableRow>
								</TableHeader>
								<TableBody>
									<For each={Array.from(eventMap.data.entries())}>{(evt) =>
										<Show when={sysEvents() || (!sysEvents() && !evt[1].issys)}>
											<TableRow class="hover:bg-yellow-100 cursor-pointer even:bg-gray-200">
												<TableCell class="py-1">{evt[1].name}</TableCell>
												<TableCell class="py-1">{evt[0]}</TableCell>
												<TableCell class="py-1 flex items-center">
													<span class="p-0 w-28"><BadgeDelta deltaType="increase">{bps_to_string(evt[1].io.write, false)}</BadgeDelta></span>
													<span class="p-0 w-28"><BadgeDelta deltaType="decrease">{bps_to_string(evt[1].io.read, false)}</BadgeDelta></span>
												</TableCell>
												<TableCell class="py-1">0</TableCell>
												<TableCell class="py-1">
													{evt[1].trigger > 0 ? `${evt[1].trigger} poll(s) ago` : "Now"}
												</TableCell>
												<TableCell class="py-1">{evt[1].issys ? "Yes" : "No"}</TableCell>
											</TableRow>
										</Show>
									}</For>
									{
									// <For each={filteredItems()}>{(item) =>
									// 	<TableRow class="hover:bg-yellow-100 cursor-pointer even:bg-gray-200" onClick={() => {
									// 		setSelectedItem(item);
									// 		if(props.onRowClick) {
									// 			props.onRowClick(item);
									// 		}
									// 	}}>
									// 		<TableCell class="py-1">{getName(item)}</TableCell>
									// 		<TableCell class="py-1">{getPath(item)}</TableCell>
									// 	</TableRow>
									// }</For>
									}
								</TableBody>
							</Table>
						</div>
					</Card>
				</Show>
			</div>
		</div>
	);
};
