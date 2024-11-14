import { MxGenericType } from './convert';

export class MxWebsocket {
	private socket: WebSocket;
	private messageid: number;
	private deferred_p: Map<number, Function>;

	public constructor(endpoint: string, port: number) {
		this.socket = new WebSocket(`ws://${endpoint}:${port}`);
		this.messageid = 0;
		this.deferred_p = new Map<number, Function>();

		this.socket.onmessage = async (message: MessageEvent) => {
			const data = JSON.parse(await message.data.text());
			if(data.type === "rpc")	{
				// Push to rpc return value queue
				if(!this.deferred_p.has(data.messageid)) {
					// Error
					console.log(`MxWebsocket did not expect server message with id: ${data.messageid}`);
				}
				else {
					const resolve = this.deferred_p.get(data.messageid);
					if(resolve) {
						resolve(this.make_rpc_response(data));
					}
					this.deferred_p.delete(data.messageid);
				}
			}
			else if(data.type === "evt") {
				// Push to subscribed events queue
				// TODO: (Cesar)
			}
		};
	}

	public async rpc_call(method: string, args: Array<MxGenericType> = [], response: boolean = true): Promise<Uint8Array> {
		const data: string = this.make_rpc_message(method, args, true);
		return new Promise<Uint8Array>((resolve) => {
			// Send the data via websocket
			this.socket.send(data);
			if(response) {
				// Defer the response
				this.deferred_p.set(this.messageid++, resolve);
			}
			else {
				// Resolve now with no response
				resolve(new Uint8Array([]));
			}
		});
	}

	// Close the connection if the ws is alive
	public close() {
		if(this.socket.readyState === WebSocket.OPEN) {
			this.socket.close();
		}
	}

	private make_rpc_message(method: string, args: Array<MxGenericType>, response: boolean) {
		const tdec = new TextDecoder();
		// Minimize network impact with b64 encoding for the arguments
		const rawData = btoa(tdec.decode(MxGenericType.concatData(args)));
		if(args.length > 0) {
			return JSON.stringify({'method': method, 'args': rawData, 'messageid': this.messageid, 'response': response});
		}
		else {
			return JSON.stringify({'method': method, 'messageid': this.messageid, 'response': response});
		}
	}

	private make_rpc_response(data: any) : Uint8Array {
		return new Uint8Array(data.response);
	}
};
