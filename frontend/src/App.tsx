import type { Component } from 'solid-js';
import Sidebar from './components/Sidebar'
import { MxWebsocket } from './lib/websocket';
import { MxGenericType } from './lib/convert';
import { createStore } from 'solid-js/store';



const App: Component = () => {

	const [mxsocket, _] = createStore(new MxWebsocket(location.hostname, parseInt(location.port)));

	function onclickf() {
		mxsocket.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512('/system/pdbloc')]).then((data) => {
			let datac = MxGenericType.fromData(data, 'generic');
			console.log(datac.astype('string'));
		});
	}
	
	function onclickf2() {
		mxsocket.rpc_call('mulex::RdbValueExists', [MxGenericType.str512('/system/pdbloc')]).then((data) => {
			let datac = MxGenericType.fromData(data, 'native');
			console.log(datac.astype('bool'));
		});
	}
	
	function onclickf3() {
		mxsocket.rpc_call('mulex::RdbValueExists', [MxGenericType.str512('/system/fail')]).then((data) => {
			let datac = MxGenericType.fromData(data, 'native');
			console.log(datac.astype('bool'));
		});
	}

	return (
		<div>
			<Sidebar/>
			<div class="container p-5">
				<button onclick={() => onclickf()}>RPC Call!</button>
				<button onclick={() => onclickf2()}>RPC Set!</button>
				<button onclick={() => onclickf3()}>RPC Get!</button>
			</div>
		</div>
		// <div class="m-left border" style="width:10%">
		// 	Sidebar
		// </div>
	);
};

export default App;
