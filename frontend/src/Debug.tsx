import { Component } from "solid-js";
import { DynamicTitle } from "./components/DynamicTitle";
import Sidebar from "./components/Sidebar";
import { MxWebsocket } from "./lib/websocket";
import { MxGenericType } from "./lib/convert";
import { createMapStore } from "./lib/rmap";

export const DebugPanel : Component = () => {

	const [clientCalls, clientCallsActions] = createMapStore<number, number>(new Map<number, number>());

	async function getRpcCallsDebugInfo() {
		const data = await MxWebsocket.instance.rpc_call('mulex::RpcGetCallsDebugData', [], 'generic');
		const calls = data.unpack(['uint64', 'uint16', 'uint64']);

		for(const call of calls) {
			const [cid, pid, count] = call;
			if(!clientCalls.data.has(cid)) {
				clientCallsActions.add(cid, count);
				continue;
			}

			clientCallsActions.modify(cid, clientCalls.data.get(cid)! + count);
		}
	}

	return (
		<div>
			<DynamicTitle title="Debug"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				Welcome to the debug page!
			</div>
		</div>
	);
};
