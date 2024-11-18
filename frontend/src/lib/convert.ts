export class MxGenericType
{
	public readonly data: Uint8Array;
	private intype: string;

	private constructor(data: Uint8Array, intype: string = 'native') {
		this.data = data;
		this.intype = intype; // 'native' for byte-by-byte repr
							  // 'generic' for size + byte-by-byte repr
	}

	static fromData(data: Uint8Array, intype: string = 'native') : MxGenericType {
		return new MxGenericType(data, intype);
	}

	// Shorthands for fromValue
	static str32(value: string, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'string32', intype)
	}

	static str512(value: string, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'string512', intype)
	}

	static f32(value: string, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'float32', intype)
	}

	static f64(value: string, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'float64', intype)
	}

	static bool(value: boolean, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'bool', intype)
	}

	static fromValue(value: any, type: string, intype: string = 'native') : MxGenericType {
		let data: Uint8Array;
		if(type === 'string32') {
			const encoder = new TextEncoder();
			data = new Uint8Array(32);
			data.set(encoder.encode(value + '\0'), 0);
		}
		else if(type === 'string512') {
			const encoder = new TextEncoder();
			data = new Uint8Array(512);
			data.set(encoder.encode(value + '\0'), 0);
		}
		else if(type === 'float32') {
			data = new Uint8Array((new Float32Array([value])).buffer);
		}
		else if(type === 'float64') {
			data = new Uint8Array((new Float64Array([value])).buffer);
		}
		else if(type === 'bool') {
			data = new Uint8Array(1);
			data.set([value ? 1 : 0], 0);
		}
		else {
			console.log('MxGenericType.fromValue: Could not convert from unknown type <' + type + '>.');
			data = new Uint8Array([]);
		}

		// Append size before the intype if the type is generic
		if(intype === 'generic') {
			let ndata = new Uint8Array(data.length + 8); // 64-bit unsigned size
			let sdata = new BigUint64Array([BigInt(data.length)]);
			ndata.set(new Uint8Array(sdata.buffer), 0);
			ndata.set(data, 8);
			return new MxGenericType(ndata, intype);
		}

		return new MxGenericType(data, intype);
	}

	static concatData(args: Array<MxGenericType>) : Uint8Array {
		// Skip if we only have one argument (this won't be that uncommon)
		if(args.length === 1) {
			return args[0].data;
		}

		// Get total length
		let length = 0;
		args.forEach(x => { length += x.data.length; });

		// Copy to new buffer and return
		let mdata = new Uint8Array(length);
		let offset = 0;
		args.forEach(x => { mdata.set(x.data, offset); offset += x.data.length; });
		return mdata;
	}

	public astype(type: string) : any {
		if(this.data.byteLength === 0) {
			return null;
		}

		let offset = 0;

		if(this.intype === 'generic' && this.data.byteLength > 8) {
			offset = 8; // 64-bit uint with size first
		}

		const data = new Uint8Array(this.data.buffer, offset);
		const view = new DataView(this.data.buffer, offset);

		// TODO: (Cesar) Check the length of data (should be superfluous...)

		// Client side is ok with only 'string'
		if(type === 'string') {
			const decoder = new TextDecoder();
			return decoder.decode(data).split('\0').shift(); // Null terminate the string
		}
		else if(type === 'float32') {
			const f32arr = new Float32Array(data);
			if(f32arr.length === 1) {
				return f32arr[0];
			}
			else {
				return Array.from(f32arr);
			}
		}
		else if(type === 'float64') {
			const f64arr = new Float64Array(data);
			if(f64arr.length === 1) {
				return f64arr[0];
			}
			else {
				return Array.from(f64arr);
			}
		}
		else if(type === 'int32') {
			return view.getUint32(0, true);
			console.log(data);
			const i32arr = new Int32Array(data.buffer);
			console.log(i32arr);
			if(i32arr.length === 1) {
				return i32arr[0];
			}
			else {
				return Array.from(i32arr);
			}
		}
		else if(type === 'bool') {
			if(data.length === 1) {
				return (data[0] === 1);
			}
			else {
				return Array.from([...data].map(Boolean));
			}
		}
		else {
			// TODO: (Cesar) Emit frontend errors to the frontend (toast, etc, ...)
			console.log('MxGenericType.astype: Could not convert from unknown type <' + type + '>.');
			return null;
		}
	}
};
