import { MxGenericType } from './convert';
import { MxWebsocket } from './websocket';

export class MxMessage {
	public contructor() {
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
			const size = view.getBigUint64(offset, true); offset += 8;
			const msg  = data.subarray(32);

			console.log(cid, ts, type, size, msg);
		});
	}
};
