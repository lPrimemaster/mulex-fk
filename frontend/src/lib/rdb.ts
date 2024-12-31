import { MxWebsocket } from './websocket';
import { MxGenericType } from './convert';

export class MxRdb {
	private root: string;

	public constructor(root: string = '') {
		this.root = root;
	}

	public watch(key: string, callback: Function) {
		const tkey = this.root + key;
		MxWebsocket.instance.rpc_call('mulex::RdbWatch', [MxGenericType.str512(tkey)]).then((response) => {
			MxWebsocket.instance.subscribe(response.astype('string'), (data: Uint8Array) => {
				const key = MxGenericType.fromData(data).astype('string');
				const value = MxGenericType.fromData(data.subarray(512), 'generic');
				callback(key, value);
				// console.log(response.astype('string'), key, value.astype('float64'), value.astype('uint64'));
			});
		});
	}

	public unwatch(key: string) {
		const tkey = this.root + key;
		MxWebsocket.instance.rpc_call('mulex::RdbWatch', [MxGenericType.str512(tkey)]).then((response) => {
			MxWebsocket.instance.unsubscribe(response.astype('string'));
		});
	}
};
