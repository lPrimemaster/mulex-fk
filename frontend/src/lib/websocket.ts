import { MxGenericType } from './convert';

export class MxWebsocket {
	private socket: WebSocket;
	private messageid: number;
	private deferred_p: Map<number, [Function, Function, string]>;
	private isready: boolean;
	private waiting_p: Array<Function>;
	private static s_instance: MxWebsocket;
	private address: string;
	private on_change: Array<Function>;
	private event_subscriptions: Map<string, Function | undefined>;

	private setupOnClose(reconnect: boolean) {
		this.socket.onclose = async () => {
			// Reset fields
			this.on_change.forEach(x => x(false));
			this.messageid = 0;
			this.deferred_p = new Map<number, [Function, Function, string]>();
			this.isready = false;
			this.waiting_p = new Array<Function>();

			if(reconnect) {
				// Attempt reconnect
				setTimeout(() => {
					this.socket = new WebSocket(`${this.address}`);
					this.setupCallbacks();
				}, 5000);
			}
		};
	}

	private setupCallbacks() {
		this.socket.onopen = async () => {
			this.isready = true;
			this.waiting_p.forEach(r => r());
			this.waiting_p = [];
			this.on_change.forEach(x => x(true));
		};

		this.socket.onmessage = async (message: MessageEvent) => {
			const data = JSON.parse(await message.data.text());
			let fail = false;

			if(data.type === "rpc")	{
				if(data.status != "OK") {
					console.log(`MxWebsocket did not execute call.`);
					console.log('Reason: ', data.status);
					fail = true;
				}

				// Push to rpc return value queue
				if(!this.deferred_p.has(data.messageid)) {
					// Error
					console.log(`MxWebsocket did not expect server message with id: ${data.messageid}`);
					console.log('Data: ', data);
				}
				else {
					const resolve = this.deferred_p.get(data.messageid);
					if(resolve) {
						if(!fail) {
							resolve[0](this.make_rpc_response(data, resolve[2]));
						}
						else {
							resolve[1]("No permissions to execute.");
						}
					}
					this.deferred_p.delete(data.messageid);
				}
			}
			else if(data.type === "evt") {
				// Push to subscribed events queue
				const callback = this.event_subscriptions.get(data.event);
				if(callback) {
					// Events could have raw data (just pass it like so)
					callback(new Uint8Array(data.response));
				}
			}
		};
		
		// Setup on close with reconnect behaviour enabled
		this.setupOnClose(true);
	}

	private constructor(endpoint: string, port: number, https: boolean) {
		if(https) {
			// port does not matter via https, we just forward!
			this.address = `wss://${endpoint}/ws/`;
		}
		else {
			this.address = `ws://${endpoint}:${port}`;
		}
		this.socket = new WebSocket(`${this.address}`);
		this.messageid = 0;
		this.deferred_p = new Map<number, [Function, string]>();
		this.isready = false;
		this.waiting_p = new Array<Function>();
		this.on_change = new Array<Function>();
		this.event_subscriptions = new Map<string, Function>();

		this.setupCallbacks();
	}

	public on_connection_change(callback: Function): void {
		this.on_change.push(callback);
	}

	public static get instance(): MxWebsocket {
		if(!MxWebsocket.s_instance) {
			const port = parseInt(location.port);
			MxWebsocket.s_instance = new MxWebsocket(location.hostname, isNaN(port) ? 80 : port, location.protocol === "https:");
		}
		return MxWebsocket.s_instance;
	}

	public get status(): number {
		return this.socket.readyState;
	}

	public async when_ready(): Promise<void> {
		return new Promise<void>((resolve) => {
			if(this.isready) {
				resolve();
			}
			else {
				this.waiting_p.push(resolve);
			}
		});
	}

	public async rpc_call(method: string, args: Array<MxGenericType> = [], response: string = 'native'): Promise<MxGenericType> {
		const [data, id] = this.make_rpc_message(method, args, response !== 'none');
		if(!this.isready) {
			await this.when_ready();
		}
		return new Promise<MxGenericType>((resolve, reject) => {
			// Send the data via websocket
			this.socket.send(data);

			// Defer the response
			this.deferred_p.set(id, [resolve, reject, response]);
			// if(response !== 'none') {
			// 	// Defer the response
			// }
			// else {
			// 	// Resolve now with no response
			// 	resolve(MxGenericType.fromData(new Uint8Array([]), response));
			// }
		});
	}

	public async subscribe(event: string, callback: Function | undefined) {
		const data: string = this.make_evt_message(event, 0);
		if(!this.isready) {
			await this.when_ready();
		}
		return new Promise<MxGenericType>((resolve) => {
			// Send the data via websocket
			this.socket.send(data);
			// Set event on map
			this.event_subscriptions.set(event, callback);
			// Resolve now with no response
			resolve(MxGenericType.fromData(new Uint8Array([])));
		});
	}

	public async unsubscribe(event: string) {
		const data: string = this.make_evt_message(event, 1);
		if(!this.isready) {
			await this.when_ready();
		}
		return new Promise<MxGenericType>((resolve) => {
			// Send the data via websocket
			this.socket.send(data);
			// Remove event from map
			this.event_subscriptions.delete(event);
			// Resolve now with no response
			resolve(MxGenericType.fromData(new Uint8Array([])));
		});
	}

	// Close the connection if the ws is alive
	public close() {
		if(this.socket.readyState === WebSocket.OPEN) {
			// Manual on close does not want a reconnect behaviour
			this.setupOnClose(false);
			this.socket.close();
		}
	}

	private make_rpc_message(method: string, args: Array<MxGenericType>, response: boolean): [string, number] {
		const id = this.messageid++;
		// const tdec = new TextDecoder('iso8859-2');
		// const tdec = new TextDecoder();

		// Minimize network impact with b64 encoding for the arguments
		const CHUNK_SIZE = 0x8000;
		let string = '';
		const buffer = MxGenericType.concatData(args);
		for(let i = 0; i < buffer.length; i+= CHUNK_SIZE) {
			string += String.fromCharCode.apply(null, Array.from(buffer.subarray(i, i + CHUNK_SIZE)));
		}
		const rawData = btoa(string);
		// const rawData = btoa(tdec.decode(MxGenericType.concatData(args)));

		if(args.length > 0) {
			// console.log(MxGenericType.concatData(args));
			return [JSON.stringify({'type': 0, 'method': method, 'args': rawData, 'messageid': id, 'response': response}), id];
		}
		else {
			return [JSON.stringify({'type': 0, 'method': method, 'messageid': id, 'response': response}), id];
		}
	}

	private make_rpc_response(data: any, response: string) : MxGenericType {
		return MxGenericType.fromData(new Uint8Array(data.response), response);
	}

	private make_evt_message(event: string, opcode: number) {
		return JSON.stringify({'type': 1, 'opcode': opcode, 'event': event});
	}
};
