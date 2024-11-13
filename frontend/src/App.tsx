import type { Component } from 'solid-js';
import Sidebar from './components/Sidebar'
import { MxWebsocket } from './lib/websocket';

let mxsocket: MxWebsocket = new MxWebsocket(location.hostname, 8080);

function onclickf() {
	let string_b = new Array(512);
	const str = '/system/pdbloc\0';
	for(let i = 0; i < str.length; i++) {
		string_b[i] = str.charCodeAt(i) & 0xFF;
	}
	mxsocket.rpc_call('mulex::RdbReadValueDirect', string_b).then((data) => {
		console.log('data:', data);
	});
}

const App: Component = () => {
	return (
		<div>
			<Sidebar/>
			<div class="container p-5">
				<button onclick={() => onclickf()}>RPC Call!</button>
			</div>
		</div>
		// <div class="m-left border" style="width:10%">
		// 	Sidebar
		// </div>
	);
};

export default App;
