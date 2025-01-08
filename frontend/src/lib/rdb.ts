import { MxWebsocket } from './websocket';
import { MxGenericType } from './convert';

export class MxRdb {
	private root: string;
	private types: Map<string, string>;

	// MxRdb fetches key types automatically for type inference on read/write
	// leveraging ts type [any]
	private async getKeyType(key: string) : Promise<string> {
		if(this.types.has(key)) {
			return this.types.get(key)!;
		}
		else {
			const typeid = (await MxWebsocket.instance.rpc_call('mulex::RdbGetKeyType', [MxGenericType.str512(key)])).astype('uint8');
			const type = MxGenericType.typeFromTypeid(typeid);
			this.types.set(key, type);
			return type;
		}
	}

	public constructor(root: string = '') {
		this.root = root;
		this.types = new Map<string, string>();
	}

	public async read(key: string) : Promise<any> {
		const tkey = this.root + key;
		const type = await this.getKeyType(tkey);
		const value = await MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(tkey)], 'generic');
		const output = value.astype(type);
		if(output === null) {
			console.error('Failed to read key [' + tkey + '].');
		}
		return output;
	}

	public async write(key: string, value: any) {
		const tkey = this.root + key;
		const type = await this.getKeyType(tkey);
		if(type.length === 0) {
			console.error('Failed to write key [' + tkey + '].');
		}
		else {
			MxWebsocket.instance.rpc_call('mulex::RdbWriteValueDirect', [MxGenericType.str512(tkey), MxGenericType.fromValue(value, type, 'generic')], 'none');
		}
	}

	public async create(key: string, type: string, value: any, count: number = 0) {
		const tkey = this.root + key;
		const ok = (await MxWebsocket.instance.rpc_call('mulex::RdbCreateValueDirect', [
			MxGenericType.str512(tkey),
			MxGenericType.uint8(MxGenericType.typeidFromType(type)),
			MxGenericType.uint64(BigInt(count)),
			MxGenericType.fromValue(value, type, 'generic')
		])).astype('bool');

		if(!ok) {
			console.error('Failed to create key [' + tkey + '].');
		}
	}

	public delete(key: string) {
		const tkey = this.root + key;
		MxWebsocket.instance.rpc_call('mulex::RdbDeleteValueDirect', [MxGenericType.str512(tkey)], 'none');
	}

	public watch(key: string, callback: Function) {
		const tkey = this.root + key;
		MxWebsocket.instance.rpc_call('mulex::RdbWatch', [MxGenericType.str512(tkey)]).then((response) => {
			MxWebsocket.instance.subscribe(response.astype('string'), (data: Uint8Array) => {
				const skey = MxGenericType.fromData(data).astype('string');
				const svalue = MxGenericType.fromData(data.subarray(512), 'generic');
				callback(skey, svalue);
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
