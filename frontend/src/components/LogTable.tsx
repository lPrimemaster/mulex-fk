import { Component, For, createSignal, createEffect, createContext, JSXElement, Setter, useContext, Accessor } from "solid-js";
import { MxWebsocket } from "../lib/websocket";
import { MxGenericType } from "~/lib/convert";
import { timestamp_tolocaltime } from "~/lib/utils";
import { showToast } from "./ui/toast";
import { BadgeLabel } from "./ui/badge-label";

export interface MxLog {
	timestamp: string;
	backend: string;
	type: string;
	message: string;
	displayToast: boolean;
};

export interface MxLogContext {
	logs: Accessor<Array<MxLog>>;
	setLogs: Setter<Array<MxLog>>;
};

const LogContext = createContext<MxLogContext>();
export const LogProvider: Component<{ children: JSXElement, maxLogs: number }> = (props) => {
	const [logs, setLogs] = createSignal<Array<MxLog>>([]);
	const clientMsgList = new Map<BigInt, string>();

	function typeToString(type: number): string {
		switch(type) {
			case 0: return 'info';
			case 1: return 'warn';
			case 2: return 'error';
		}
		return '';
	}

	createEffect(() => {
		const llist = logs();
		if(llist.length == 0) {
			return;
		}

		const log = llist[llist.length - 1];

		if(log.type == 'error' && log.displayToast) {
			showToast({
				title: '[' + log.timestamp + '] Error at ' + log.backend,
				description: log.message,
				variant: 'error'
			});
		}
	});

	function addMessage(cid: BigInt, ts: BigInt, type: number, message: string, displayToast: boolean = true) {
		// MEMO
		// enum class MsgClass : std::uint8_t
		// {
		// 	INFO,
		// 	WARN,
		// 	ERROR
		// };
		if(!clientMsgList.has(cid)) {
			MxWebsocket.instance.rpc_call(
				'mulex::RdbReadValueDirect',
				[MxGenericType.str512('/system/backends/' + cid.toString(16) + '/name')],
				'generic'
			).then((res: MxGenericType) => {
				const bck_name: string = res.astype('string');
				clientMsgList.set(cid, bck_name);

				setLogs((prev) => {
					const log = {
						timestamp: timestamp_tolocaltime(Number(ts)),
						backend: bck_name,
						type: typeToString(type),
						message: message,
						displayToast: displayToast
					};
					const nlogs = [...prev, log];
					return props.maxLogs ? nlogs.slice(-props.maxLogs) : nlogs;
				});
			});
		}
		else {
			const bck_name = clientMsgList.get(cid)!;
			setLogs((prev) => {
				const log = {
					timestamp: timestamp_tolocaltime(Number(ts)),
					backend: bck_name,
					type: typeToString(type),
					message: message,
					displayToast: displayToast
				};
				const nlogs = [...prev, log];
				return props.maxLogs ? nlogs.slice(-props.maxLogs) : nlogs;
			});
		}
	}

	MxWebsocket.instance.on_connection_change((conn: boolean) => {
		if(conn) {
			// Fetch the logs
			MxWebsocket.instance.rpc_call('mulex::MsgGetLastLogs', [MxGenericType.uint32(props.maxLogs)], 'generic').then((res: MxGenericType) => {
				const logs = res.unpack(['int32', 'uint8', 'uint64', 'int64', 'str512']);
				for(const l of logs.reverse()) {
					const [ _, level, cid, timestamp, message ] = l;
					addMessage(cid, timestamp, level, message, false);
				}
			});

			// Watch on new logs
			MxWebsocket.instance.subscribe('mxmsg::message', (data: Uint8Array) => {
				// MEMO:
				// struct MsgMessage
				// {
				// 	std::uint64_t _client;
				// 	std::int64_t  _timestamp;
				// 	MsgClass 	  _type;
				//  std::uint8_t  _padding[7];
				// 	std::uint64_t _size;
				// 	char		  _message[];
				// };

				const view = new DataView(data.buffer);
				let offset = 0;
				const cid  = view.getBigUint64(offset, true); offset += 8;
				const ts   = view.getBigInt64(offset, true); offset += 8;
				const type = view.getUint8(offset); offset += 8; // With padding
				// const size = view.getBigUint64(offset, true); offset += 8;
				const msg  = data.subarray(32, -1);

				const decoder = new TextDecoder('latin1');
				const message = decoder.decode(msg);

				addMessage(cid, ts, type, message);
			});
		}
	});

	return <LogContext.Provider value={{ logs: logs, setLogs: setLogs }}>{props.children}</LogContext.Provider>
};

export const LogTable: Component = () => {
	const { logs } = useContext(LogContext) as MxLogContext;
	let ref!: HTMLDivElement;

	createEffect(() => {
		if(logs().length > 0) {
			ref.scrollTo({ top: ref.scrollHeight });
		}
	});

	return (
		<div class="border bg-white rounded-md md:h-40 lg:h-64 overflow-y-scroll" ref={ref}>
			<For each={logs()}>{(log: MxLog) => {
				const bg_color = (log.type == 'error') ? 'bg-red-100' : (log.type == 'warn') ? 'bg-yellow-100' : '';
				const class_style = 'px-1 py-0.5 border-b border-gray-300 ' + bg_color;
				const label_type = (log.type == 'error') ? 'error' : (log.type == 'warn') ? 'warning' : 'display';
				return (
					<div class={class_style}>
						<BadgeLabel class="py-0.5 px-1 place-content-center" style="width: 4rem;" type="display">{log.timestamp}</BadgeLabel>
						<BadgeLabel class="py-0.5 px-1 ml-1" type="display">{log.backend.length > 0 ? log.backend : '<unknown>'}</BadgeLabel>
						<BadgeLabel class="py-0.5 px-1 ml-1" type={label_type}>{log.type}</BadgeLabel>
						<span class="ml-1 py-0.5">{log.message}</span>
					</div>
				);
			}}</For>
		</div>
	);
};
