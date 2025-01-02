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
		return MxGenericType.fromValue(value, 'string32', intype);
	}

	static str512(value: string, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'string512', intype);
	}

	static int8(value: number, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'int8', intype);
	}

	static uint8(value: number, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'uint8', intype);
	}

	static int32(value: number, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'int32', intype);
	}

	static uint32(value: number, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'uint32', intype);
	}

	static uint64(value: BigInt, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'uint64', intype);
	}

	static f32(value: number, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'float32', intype);
	}

	static f64(value: number, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'float64', intype);
	}

	static bool(value: boolean, intype: string = 'native') : MxGenericType {
		return MxGenericType.fromValue(value, 'bool', intype);
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
		else if(type === 'int8') {
			data = new Uint8Array((new Int8Array([value])).buffer);
		}
		else if(type === 'int16') {
			data = new Uint8Array((new Int16Array([value])).buffer);
		}
		else if(type === 'int32') {
			data = new Uint8Array((new Int32Array([value])).buffer);
		}
		else if(type === 'int64') {
			data = new Uint8Array((new BigInt64Array([BigInt(value)])).buffer);
		}
		else if(type === 'uint8') {
			data = new Uint8Array([value]);
		}
		else if(type === 'uint16') {
			data = new Uint8Array((new Uint16Array([value])).buffer);
		}
		else if(type === 'uint32') {
			data = new Uint8Array((new Uint32Array([value])).buffer);
		}
		else if(type === 'uint64') {
			data = new Uint8Array((new BigUint64Array([BigInt(value)])).buffer);
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

	private splitChunks(args: Uint8Array, chunksize: number) : Array<Uint8Array> {
		const chunks = new Array<Uint8Array>();

		for(let i = 0; i < args.length; i += chunksize) {
			chunks.push(args.slice(i, i + chunksize));
		}

		return chunks;
	}

	static typeFromTypeid(id: number): string {
		switch(id) {
			case  0: { return 'int8'; }
			case  1: { return 'int16'; }
			case  2: { return 'int32'; }
			case  3: { return 'int64'; }
			case  4: { return 'uint8'; }
			case  5: { return 'uint16'; }
			case  6: { return 'uint32'; }
			case  7: { return 'uint64'; }
			case  8: { return 'float32'; }
			case  9: { return 'float64'; }
			case 10: { return 'string'; }
			case 11: { return 'bool'; }
		}
		return '';
	}

	static typeidFromType(type: string): number {
		switch(type.toLowerCase()) {
			case 'int8'   : { return  0; }
			case 'int16'  : { return  1; }
			case 'int32'  : { return  2; }
			case 'int64'  : { return  3; }
			case 'uint8'  : { return  4; }
			case 'uint16' : { return  5; }
			case 'uint32' : { return  6; }
			case 'uint64' : { return  7; }
			case 'float32': { return  8; }
			case 'float64': { return  9; }
			case 'string' : { return 10; }
			case 'bool'   : { return 11; }
		}
		return NaN;
	}

	public hexdump() : Array<string> {
		if(this.data.byteLength === 0) {
			return [];
		}

		let offset = 0;
		if(this.intype === 'generic' && this.data.byteLength > 8) {
			offset = 8; // 64-bit uint with size first
		}

		return Array.from(this.data.slice(offset)).map((b) => '0x' + b.toString(16).padStart(2, '0')).reverse();
	}

	public astype(type: string) : any {
		if(this.data.byteLength === 0) {
			return null;
		}

		let offset = 0;

		if(this.intype === 'generic' && this.data.byteLength > 8) {
			offset = 8; // 64-bit uint with size first
		}

		offset += this.data.byteOffset;

		const data = new Uint8Array(this.data.buffer, offset);

		// TODO: (Cesar) Check the length of data (should be superfluous...)

		// Client side is ok with only 'string'
		if(type === 'string') {
			const decoder = new TextDecoder();
			return decoder.decode(data).split('\0').shift(); // Null terminate the string
		}
		else if(type === 'stringarray') {
			const decoder = new TextDecoder();

			const array: Array<Uint8Array> = this.splitChunks(data, 512);
			const out = new Array<string>();
			for(let i = 0; i < array.length; i++) {
				let element = decoder.decode(array[i]).split('\0').shift();
				if(element) {
					out.push(element);
				}
			}
			return out;

			// Strings are all null terminated on the backend so we can instead do:
			// return decoder.decode(data).split('\0');
		}
		else if(type === 'float32') {
			const f32arr = new Float32Array(data.buffer, offset);
			if(f32arr.length === 1) {
				return f32arr[0];
			}
			else {
				return Array.from(f32arr);
			}
		}
		else if(type === 'float64') {
			const f64arr = new Float64Array(data.buffer, offset);
			if(f64arr.length === 1) {
				return f64arr[0];
			}
			else {
				return Array.from(f64arr);
			}
		}
		else if(type === 'int8') {
			const i8arr = new Int8Array(data.buffer, offset);
			if(i8arr.length === 1) {
				return i8arr[0];
			}
			else {
				return Array.from(i8arr);
			}
		}
		else if(type === 'uint8') {
			const u8arr = new Int8Array(data.buffer, offset);
			if(u8arr.length === 1) {
				return u8arr[0];
			}
			else {
				return Array.from(u8arr);
			}
		}
		else if(type === 'int16') {
			const i16arr = new Int16Array(data.buffer, offset);
			if(i16arr.length === 1) {
				return i16arr[0];
			}
			else {
				return Array.from(i16arr);
			}
		}
		else if(type === 'uint16') {
			const ui16arr = new Uint16Array(data.buffer, offset);
			if(ui16arr.length === 1) {
				return ui16arr[0];
			}
			else {
				return Array.from(ui16arr);
			}
		}
		else if(type === 'int32') {
			const i32arr = new Int32Array(data.buffer, offset);
			if(i32arr.length === 1) {
				return i32arr[0];
			}
			else {
				return Array.from(i32arr);
			}
		}
		else if(type === 'uint32') {
			const ui32arr = new Uint32Array(data.buffer, offset);
			if(ui32arr.length === 1) {
				return ui32arr[0];
			}
			else {
				return Array.from(ui32arr);
			}
		}
		else if(type === 'int64') {
			const view = new DataView(data.buffer, offset);
			return view.getBigInt64(0, true);
		}
		else if(type === 'uint64') {
			const view = new DataView(data.buffer, offset);
			return view.getBigUint64(0, true);
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

	public unpack(structure: Array<string>) : any {
		if(this.data.byteLength === 0) {
			return null;
		}

		let offset = 0;

		if(this.intype === 'generic' && this.data.byteLength > 8) {
			offset = 8; // 64-bit uint with size first
		}

		offset += this.data.byteOffset;

		const view = new DataView(this.data.buffer, offset);
		let packoffset = 0;
		const output = new Array<Array<any>>();

		// TODO: (Cesar) Check the length of data (should be superfluous...)

		while(packoffset < this.data.length - offset) {
			const element = new Array<any>();
			for(const type of structure) {
				// Client side is ok with only 'string'
				if(type === 'str512') {
					const decoder = new TextDecoder();
					element.push(decoder.decode(view.buffer.slice(view.byteOffset + packoffset)).split('\0').shift()); // Null terminate the string
					packoffset += 512;
				}
				else if(type === 'str32') {
					const decoder = new TextDecoder();
					element.push(decoder.decode(view.buffer.slice(view.byteOffset + packoffset)).split('\0').shift()); // Null terminate the string
					packoffset += 32;
				}
				else if(type === 'float32') {
					element.push(view.getFloat32(packoffset, true));
					packoffset += 4;
				}
				else if(type === 'float64') {
					element.push(view.getFloat64(packoffset, true));
					packoffset += 8;
				}
				else if(type === 'int8') {
					element.push(view.getInt8(packoffset));
					packoffset += 1;
				}
				else if(type === 'uint8') {
					element.push(view.getUint8(packoffset));
					packoffset += 1;
				}
				else if(type === 'int16') {
					element.push(view.getInt16(packoffset, true));
					packoffset += 2;
				}
				else if(type === 'uint16') {
					element.push(view.getUint16(packoffset, true));
					packoffset += 2;
				}
				else if(type === 'int32') {
					element.push(view.getInt32(packoffset, true));
					packoffset += 4;
				}
				else if(type === 'uint32') {
					element.push(view.getUint32(packoffset, true));
					packoffset += 4;
				}
				else if(type === 'int64') {
					element.push(view.getBigInt64(packoffset, true));
					packoffset += 8;
				}
				else if(type === 'uint64') {
					element.push(view.getBigUint64(packoffset, true));
					packoffset += 8;
				}
				else if(type === 'bool') {
					element.push(view.getUint8(packoffset) === 1);
					packoffset += 1;
				}
				else {
					// TODO: (Cesar) Emit frontend errors to the frontend (toast, etc, ...)
					console.log('MxGenericType.unpack: Could not convert from unknown type <' + type + '>.');
					return null;
				}
			}
			output.push(element);
		}
		return output;
	}
};
