import { Component, createMemo, createSignal } from "solid-js";
import { DynamicTitle } from "./components/DynamicTitle";
import Sidebar from "./components/Sidebar";
import { TraceTimeline, TraceRecord } from "./components/TraceTimeline";
import { MxPopup } from "./components/Popup";
import { formatTime } from "./lib/utils";

export const TraceViewer: Component = () => {
	const [record, setRecord] = createSignal<TraceRecord | undefined>(undefined);
	const [recordClientName, setRecordClientName] = createSignal<string>('');

	function setRecordAndClientName(record: TraceRecord, name: string) {
		setRecord(record);
		setRecordClientName(name);
	}

	function getRecordFieldSafe<T>(field: string) {
		const r = record();
		if(!r) return '';
		return r[field as keyof TraceRecord] as T;
	}

	// TODO: (César) This will not work
	// 				 the providing side of the record
	// 				 on the TraceTimeline is not tracked
	const recordTimestamp = createMemo(() => {
		const r = record();
		if(!r) return '';
		if(!r.complete) return 'Still running...';
		return formatTime((r.timestamp.end - r.timestamp.begin) * 1_000_000, 3, true);
	});

	const recordStartTime = createMemo(() => {
		const r = record();
		if(!r) return '';
		return formatTime(r.timestamp.begin * 1_000_000, 3, true);
	});

	return (
		<div>
			<DynamicTitle title="Tracing"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<TraceTimeline onRecordSelect={setRecordAndClientName}/>
			</div>
			<MxPopup title='Trace Record' open={record() !== undefined} onOpenChange={() => setRecord(undefined)}>
				<div class="grid grid-rows-6 grid-cols-2 gap-2">
					<span class="font-bold">Group</span>
					<span>{getRecordFieldSafe<string>('group')}</span>

					<span class="font-bold">Name</span>
					<span>{getRecordFieldSafe<string>('name')}</span>

					<span class="font-bold">Trace ID</span>
					<span>{'0x' + getRecordFieldSafe<BigInt>('traceid').toString(16).toUpperCase()}</span>

					<span class="font-bold">Source Client</span>
					<span>{recordClientName() + ' [0x' + getRecordFieldSafe<BigInt>('clientid').toString(16) + ']'}</span>

					<span class="font-bold">Start Relative Time</span>
					<span>{recordStartTime()}</span>

					<span class="font-bold">Execution Time</span>
					<span>{recordTimestamp()}</span>
				</div>
			</MxPopup>
		</div>
	);
};
