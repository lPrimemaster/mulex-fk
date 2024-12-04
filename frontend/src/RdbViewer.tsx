import { Component, useContext, createSignal, createEffect, For, Show } from 'solid-js';
import { createStore } from 'solid-js/store';
import Sidebar from './components/Sidebar';
import Card from './components/Card';
import { SearchBar, SearchBarProvider, SearchBarContext, SearchBarContextType } from './components/SearchBar';
import { MxWebsocket } from './lib/websocket';
import { MxGenericType } from './lib/convert';
import { createSetStore } from './lib/rset';
import { BadgeDelta } from './components/ui/badge-delta';
import { BadgeLabel } from './components/ui/badge-label';
import { MxRdb } from './lib/rdb';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from './components/ui/table';
import { ComboSimple } from './components/ComboSimple';
import { Transition } from 'solid-transition-group';
import { Button } from './components/Button';
import { Dialog, DialogContent, DialogDescription, DialogFooter, DialogHeader, DialogTitle, DialogTrigger } from './components/ui/dialog';
import { TextField, TextFieldLabel, TextFieldInput } from './components/ui/text-field';

class RdbStats {
	read: number;
	write: number;
	nkeys: BigInt;
	size: BigInt;
	allocated: BigInt;

	constructor(read: number, write: number, nkeys: BigInt, size: BigInt, allocated: BigInt) {
		this.read = read;
		this.write = write;
		this.nkeys = nkeys;
		this.size = size;
		this.allocated = allocated;
	}

	static copy(other: RdbStats): RdbStats {
		return new RdbStats(other.read, other.write, other.nkeys, other.size, other.allocated);
	}
};

const [rdbKeys, rdbKeysAction] = createSetStore<string>();
const [rdbStats, setRdbStats] = createStore<RdbStats>(new RdbStats(0, 0, BigInt(0), BigInt(0), BigInt(0)));

// const RdbKeyProperty: Component<{label: string, value: string}> = (props) => {
// 	return (
// 		<div class="flex gap-2">
// 			<span class="w-20 text-right">{props.label}</span>
// 			<span><BadgeLabel type="display">{props.value}</BadgeLabel></span>
// 		</div>
// 	);
// };

const RdbKeyDisplay: Component = () => {
	const { selectedItem, setSelectedItem } = useContext(SearchBarContext) as SearchBarContextType;
	const [path, setPath] = createSignal<string>('');
	const [name, setName] = createSignal<string>('');
	const [type, setType] = createSignal<string>('');
	const [value, setValue] = createSignal<string>('');
	const [size, setSize] = createSignal<string>('');
	const [rawHexArray, setRawHexArray] = createSignal<Array<string>>([]);
	const [isArray, setIsArray] = createSignal<string>('');
	const [isActive, setIsActive] = createSignal<boolean>(false);
	const [openEdit, setOpenEdit] = createSignal<boolean>(false);
	const [openDelete, setOpenDelete] = createSignal<boolean>(false);
	const [writeValue, setWriteValue] = createSignal<string | number>('');
	const rdb = new MxRdb();
	let last_resolve = true;

	createEffect((previous: string) => {
		const itemActive = (selectedItem().length > 0);
		setIsActive(itemActive);
		if(itemActive) {
			if(previous.length > 0) {
				rdb.unwatch(previous);
			}
			const parr = selectedItem().split('/');
			setPath(parr.slice(0, -1).join('/'));
			setName(parr[parr.length - 1]);
			MxWebsocket.instance.rpc_call('mulex::RdbReadKeyMetadata', [MxGenericType.str512(selectedItem())], 'generic').then((res) => {
				const ktype = MxGenericType.typeFromTypeid(res.astype('uint8'));
				setType(ktype.toUpperCase());
				MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(selectedItem())], 'generic').then((res) => {
					setSize((res.data.length - 8).toString());
					setValue(res.astype(ktype).toString());
					setIsArray(value().includes(',') ? 'Yes' : 'No');
					setRawHexArray(res.hexdump());
				});

				// BUG: (Cesar) This might not show the last updated value
				rdb.watch(selectedItem(), (key: string) => {
					if(last_resolve) {
						last_resolve = false;
						MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(key)], 'generic').then((res) => {
							setValue(res.astype(ktype).toString());
							setRawHexArray(res.hexdump());
							last_resolve = true;
						});
					}
				});

				// const id = setInterval(() => {
				// 	MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(selectedItem())], 'generic').then((res) => {
				// 		setValue(res.astype(ktype).toString());
				// 		setRawHexArray(res.hexdump().split(' '));
				// 	});
				// }, 1000);
			});
			return selectedItem();
		}
		return '';
	}, '');

	return (
		<Transition
			onEnter={(el, done) => {
				const anim = el.animate([{ opacity: 0, transform: 'scale(0) translateY(-10px)' }, { opacity: 1, transform: 'scale(1) translateY(0px)' }], { duration: 100 });
				anim.finished.then(done);
			}}
			onExit={(el, done) => {
				const anim = el.animate([{ opacity: 1, transform: 'scale(1) translateY(0px)' }, { opacity: 0, transform: 'scale(0) translateY(-10px)' }], { duration: 100 });
				anim.finished.then(done);
			}}
		><Show when={isActive()}>
			<Card title="">
				<div class="flex">
					<Table>
						<TableHeader>
							<TableRow>
								<TableHead>Property</TableHead>
								<TableHead>Value</TableHead>
							</TableRow>
						</TableHeader>
						<TableBody>
							<TableRow class="hover:bg-gray-100">
								<TableCell class="w-20 p-1 pl-5">Path</TableCell>
								<TableCell class="p-1 pl-5 pr-10">{path()}</TableCell>
							</TableRow>
							<TableRow class="bg-gray-200 hover:bg-gray-200">
								<TableCell class="p-1 pl-5">Name</TableCell>
								<TableCell class="p-1 pl-5 pr-10">{name()}</TableCell>
							</TableRow>
							<TableRow class="hover:bg-gray-100">
								<TableCell class="p-1 pl-5">Value</TableCell>
								<TableCell class="p-1 pl-5 pr-10">{value()}</TableCell>
							</TableRow>
							<TableRow class="bg-gray-200 hover:bg-gray-200">
								<TableCell class="p-1 pl-5">Type</TableCell>
								<TableCell class="p-1 pl-5 pr-10">{type()}</TableCell>
							</TableRow>
							<TableRow class="bg-gray-100">
								<TableCell class="p-1 pl-5">Size</TableCell>
								<TableCell class="p-1 pl-5 pr-10">{size()}</TableCell>
							</TableRow>
							<TableRow class="bg-gray-200 hover:bg-gray-200">
								<TableCell class="p-1 pl-5">Array</TableCell>
								<TableCell class="p-1 pl-5 pr-10">
									<ComboSimple data={[
										{ label: 'Yes', value: true , disabled: false },
										{ label: 'No' , value: false, disabled: false }
									]} placeholder="" default={isArray()} disabled onSelect={(value: boolean) => { console.log('Selected ' + value); }}/>
								</TableCell>
							</TableRow>
							<TableRow class="bg-gray-100">
								<TableCell class="p-1 pl-5 align-top">Raw View</TableCell>
								<TableCell class="p-1 pl-5 pr-10 gap-5 flex flex-wrap">
									<For each={rawHexArray()}>{(byte: string) =>
										<div class="w-10">
											{byte}
										</div>
									}</For>
								</TableCell>
							</TableRow>
						</TableBody>
					</Table>
				</div>
				<div class="flex flex-row-reverse mx-10 gap-2">
					<Button onClick={() => { setSelectedItem(''); }}>Close</Button>
					<Button type="error" disabled={path().includes('system')} onClick={() => setOpenDelete(true) }>Delete</Button>
					<Button disabled={path().includes('system')} onClick={ () => setOpenEdit(true) }>Edit</Button>
				</div>
				<Dialog open={openEdit()} onOpenChange={setOpenEdit}>
					<DialogTrigger/>
					<DialogContent>
						<DialogHeader>
							<DialogTitle class="text-center">Edit '{name()}'</DialogTitle>
						</DialogHeader>
						<div class="grid gap-5 py-5">
							<TextField onChange={setWriteValue} class="grid grid-cols-4 items-center gap-5">
								<TextFieldLabel class="text-right">Value</TextFieldLabel>
								<TextFieldInput value={value()} class="col-span-3" type={type() === 'string' ? 'text' : 'number'}/>
							</TextField>
						</div>
						<DialogFooter>
							<Button onClick={() => {
								MxWebsocket.instance.rpc_call('mulex::RdbWriteValueDirect', [
									MxGenericType.str512(path() + '/' + name()),
									MxGenericType.fromValue(writeValue(), type().toLowerCase(), 'generic')
								], 'none');
								setOpenEdit(false);
							}}>Save</Button>
							<Button onClick={setOpenEdit}>Cancel</Button>
						</DialogFooter>
					</DialogContent>
				</Dialog>

				<Dialog open={openDelete()} onOpenChange={setOpenDelete}>
					<DialogTrigger/>
					<DialogContent>
						<DialogHeader>
							<DialogTitle class="text-center">Delete '{name()}'</DialogTitle>
						</DialogHeader>
						<div class="grid gap-5 py-5">
							Are you sure you wan't to delete this key?
						</div>
						<DialogFooter>
							<Button type="error" onClick={() => {
								MxWebsocket.instance.rpc_call('mulex::RdbDeleteValueDirect',
															  [MxGenericType.str512(path() + '/' + name())],
															  'none');
								setOpenDelete(false);
								setSelectedItem('');
							}}>Delete</Button>
							<Button onClick={setOpenDelete}>Cancel</Button>
						</DialogFooter>
					</DialogContent>
				</Dialog>
			</Card>
		</Show></Transition>
	);
};

export const RdbViewer: Component = () => {

	MxWebsocket.instance.on_connection_change((conn: boolean) => {
		if(conn) {
			MxWebsocket.instance.rpc_call('mulex::RdbListKeys', [], 'generic').then((data) => {
				const rdbkeys: Array<string> = data.astype('stringarray');
				rdbKeysAction.set(rdbkeys);
			});
		}
	});

	if(rdbKeys.data.size === 0) {
		MxWebsocket.instance.rpc_call('mulex::RdbListKeys', [], 'generic').then((data) => {
			const rdbkeys: Array<string> = data.astype('stringarray');
			rdbKeysAction.set(rdbkeys);
		});
	}

	MxWebsocket.instance.subscribe('mxrdb::keycreated', (data: Uint8Array) => {
		const key = MxGenericType.fromData(data).astype('string');
		rdbKeysAction.add(key);
	});

	MxWebsocket.instance.subscribe('mxrdb::keydeleted', (data: Uint8Array) => {
		const key = MxGenericType.fromData(data).astype('string');
		rdbKeysAction.remove(key);
	});

	function setRdbStat(key: string, type: string) {
		MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512('/system/rdb/statistics/' + key)], 'generic').then((res: MxGenericType) => {
			setRdbStats((p) => { const n = RdbStats.copy(p); n[key as keyof RdbStats] = res.astype(type); return n; });
		});
	}

	setRdbStat('read', 'uint32');
	setRdbStat('write', 'uint32');
	setRdbStat('nkeys', 'uint64');
	setRdbStat('allocated', 'uint64');
	setRdbStat('size', 'uint64');

	const rdb = new MxRdb();

	rdb.watch('/system/rdb/statistics/*', (key: string) => {
		MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512(key)], 'generic').then((res: MxGenericType) => {

			if(key.endsWith('read')) {
				setRdbStats((p) => { const n = RdbStats.copy(p); n.read = res.astype('uint32'); return n; });
			}
			else if(key.endsWith('write')) {
				setRdbStats((p) => { const n = RdbStats.copy(p); n.write = res.astype('uint32'); return n; });
			}
			else if(key.endsWith('nkeys')) {
				setRdbStats((p) => { const n = RdbStats.copy(p); n.nkeys = res.astype('uint64'); return n; });
			}
			else if(key.endsWith('allocated')) {
				setRdbStats((p) => { const n = RdbStats.copy(p); n.allocated = res.astype('uint64'); return n; });
			}
			else if(key.endsWith('size')) {
				setRdbStats((p) => { const n =RdbStats.copy(p); n.size = res.astype('uint64'); return n; });
			}
		});
	});

	return (
		<div>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<SearchBarProvider>
					<Card title="">
						{/*<SearchBar items={Array.from(['abc', 'def'])} placeholder="Search rdb..."/>*/}
						<SearchBar items={Array.from(rdbKeys.data)} placeholder="Search rdb..."/>
					</Card>
					<RdbKeyDisplay/>
				</SearchBarProvider>
				<Card title="Info">
					<div class="flex sm:gap-5 md:gap-10 flex-wrap">
						<div class="font-normal flex">
							<span class="px-1 text-sm">Usage</span>
							<span class="px-1"><BadgeDelta deltaType="increase">{rdbStats.read} r/s</BadgeDelta></span>
							<span class="px-1"><BadgeDelta deltaType="decrease">{rdbStats.write} w/s</BadgeDelta></span>
						</div>
						<div class="flex">
							<span>Total keys</span>
							<span class="px-1"><BadgeLabel type="display">{rdbStats.nkeys.toString()} keys</BadgeLabel ></span>
						</div>
						<div class="flex">
							<span>Size</span>
							<span class="px-1"><BadgeLabel type="display">{rdbStats.size.toString()} B</BadgeLabel ></span>
						</div>
						<div class="flex">
							<span>Allocated</span>
							<span class="px-1"><BadgeLabel type="display">{rdbStats.allocated.toString()} B</BadgeLabel ></span>
						</div>
					</div>
				</Card>
			</div>
		</div>
	);
};

