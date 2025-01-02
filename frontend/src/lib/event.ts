import { MxWebsocket } from './websocket';

type MxEventCallback = (data: Uint8Array) => void;

export class MxEvent {
	private ename: string;
	private onCloseCb: (() => void) | undefined;

	private static finalizer = new FinalizationRegistry((onClose : () => void) => {
		onClose();
	});

	public constructor(name: string) {
		this.ename = name;
	}

	public get name() {
		return this.ename;
	}

	public set onMessage(func: MxEventCallback) {
		MxWebsocket.instance.subscribe(this.ename, func);

		this.onCloseCb = () => {
			MxWebsocket.instance.unsubscribe(this.ename);
		};
		MxEvent.finalizer.register(this, this.onCloseCb);
	}
};
