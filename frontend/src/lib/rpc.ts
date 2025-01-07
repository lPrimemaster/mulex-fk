import { MxWebsocket } from './websocket';
import { MxGenericType } from './convert';
import { array_chunkify } from './utils';

export class MxRpc {

	[key: string]: any;

	public static async Create() {
		const instance = new MxRpc();
		await instance.setupCalls();
		return instance;
	}

	private async setupCalls() {
		const res = await MxWebsocket.instance.rpc_call('mulex::RpcGetAllCalls', [], 'generic');
		const methods = array_chunkify<string>(res.astype('stringarray'), 2);

		for(const method of methods) {
			// Assume all methods start with the mulex:: namespace
			const func = method[0].split('::').pop()!;
			const rettype = method[1].split('::').pop()!;

			let response = 'native';
			if(rettype.includes('void')) {
				response = 'none';
			}
			else if(rettype.includes('RPCGenericType')) {
				response = 'generic';
			}

			console.log(func);
			
			// Assume all of the function names differ
			(this as any)[func] = async (args: Array<MxGenericType>) => await MxWebsocket.instance.rpc_call(method[0], args, response);
		}
	}

	private constructor() {
	}
};
