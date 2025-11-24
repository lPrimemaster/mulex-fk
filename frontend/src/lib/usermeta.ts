import { MxGenericType } from "./convert";
import { MxWebsocket } from "./websocket";

export class MxUserMetadata {
	private static async GetAll() {
		const data = await MxWebsocket.instance.rpc_call('mulex::PdbUserMetadataGet', [], 'generic');
		const dec = new TextDecoder();
		const bdata = data.unpack(['bytearray'])[0][0];
		const json = dec.decode(bdata);
		return json.length > 0 ? JSON.parse(dec.decode(bdata)) : undefined;
	}

	private static async SendMetaObject(metadata: any) {
		const enc = new TextEncoder();
		await MxWebsocket.instance.rpc_call('mulex::PdbUserMetadataSet', [
			MxGenericType.makeData(new Uint8Array(enc.encode(JSON.stringify(metadata))), 'generic')
		], 'none');
	}

	public static async Get(key: string) {
		const object = await MxUserMetadata.GetAll();
		return object !== undefined ? object[key] : undefined;
	}

	public static async Set(key: string, value: any) {
		let metadata = await MxUserMetadata.GetAll();
		if(!metadata) {
			metadata = {};
		}
		metadata[key] = value;
		MxUserMetadata.SendMetaObject(metadata);
	}

	public static async Clear(key: string) {
		const metadata = await MxUserMetadata.GetAll();
		delete metadata[key];
		MxUserMetadata.SendMetaObject(metadata);
	}
}


