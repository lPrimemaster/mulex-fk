import { Component, useContext, createSignal, For, Show, createEffect, on, onMount } from 'solid-js';
import Sidebar from './components/Sidebar';
import { RdbPlot } from './components/RdbPlot';
import 'uplot/dist/uPlot.min.css';
import { cssColorToRGB } from './lib/utils';
import { MxButton } from './api/Button';
import Card from './components/Card';
import { Dialog, DialogContent, DialogFooter, DialogHeader, DialogTitle, DialogTrigger } from './components/ui/dialog';
import { createMapStore } from './lib/rmap';
import { TextField, TextFieldErrorMessage, TextFieldInput, TextFieldLabel } from './components/ui/text-field';
import { SearchBar, SearchBarContext, SearchBarContextType, SearchBarProvider } from './components/SearchBar';
import { MxWebsocket } from './lib/websocket';
import { MxGenericType } from './lib/convert';
import { createSetStore } from './lib/rset';
import { BadgeLabel } from './components/ui/badge-label';
import { Tooltip, TooltipContent, TooltipTrigger } from './components/ui/tooltip';
import { Checkbox } from './components/ui/checkbox';
import { MxRdb } from './lib/rdb';

interface DisplayOptions {
	color: string;
	label?: string;
	fill?: boolean;

	key: string;
	units?: string;
	dedicated_y?: boolean;
};

const [selectedKeys, selectedKeysAction] = createSetStore<string>();
const [selectedKeysData, selectedKeysDataActions] = createMapStore<string, DisplayOptions>(new Map<string, DisplayOptions>());
const [displays, displaysActions] = createMapStore<string, Array<DisplayOptions>>(new Map<string, Array<DisplayOptions>>());
const [displayName, setDisplayName] = createSignal<string>('');
const [rdbKeyTypes, rdbKeyTypesAction] = createMapStore<string, number>(new Map<string, number>());

const HistoryPlotRDBSearcher : Component<{ validityHandler: Function }> = (props) => {
	const [rdbKeys, rdbKeysAction] = createSetStore<string>();
	const { selectedItem, setSelectedItem } = useContext(SearchBarContext) as SearchBarContextType;

	MxWebsocket.instance.on_connection_change((conn: boolean) => {
		if(conn) {
			MxWebsocket.instance.rpc_call('mulex::RdbListKeys', [], 'generic').then((data) => {
				const rdbkeys: Array<string> = data.astype('stringarray');
				rdbKeysAction.set(rdbkeys);

				// This function provides the types on the same order as RdbListKeys
				MxWebsocket.instance.rpc_call('mulex::RdbListKeyTypes', [], 'generic').then((data) => {
					const rdbkeytypes: Array<number> = data.astype('uint8');

					for(let i = 0; i < rdbkeys.length; i++) {
						rdbKeyTypesAction.add(rdbkeys[i], rdbkeytypes[i]);
					}
				});
			});
		}
	});

	onMount(() => {
		MxWebsocket.instance.subscribe('mxrdb::keycreated', (data: Uint8Array) => {
			const key = MxGenericType.fromData(data).astype('string');
			rdbKeysAction.add(key);

			MxWebsocket.instance.rpc_call('mulex::RdbGetKeyType', [MxGenericType.str512(key)]).then((res: MxGenericType) => {
				rdbKeyTypesAction.add(key, res.astype('uint8'));
			});
		});

		MxWebsocket.instance.subscribe('mxrdb::keydeleted', (data: Uint8Array) => {
			const key = MxGenericType.fromData(data).astype('string');
			rdbKeysAction.remove(key);
			rdbKeyTypesAction.remove(key);
		});
	});

	if(rdbKeys.data.size === 0) {
		MxWebsocket.instance.rpc_call('mulex::RdbListKeys', [], 'generic').then((data) => {
			const rdbkeys: Array<string> = data.astype('stringarray');
			rdbKeysAction.set(rdbkeys);

			// This function provides the types on the same order as RdbListKeys
			MxWebsocket.instance.rpc_call('mulex::RdbListKeyTypes', [], 'generic').then((data) => {
				const rdbkeytypes: Array<number> = data.astype('uint8');

				for(let i = 0; i < rdbkeys.length; i++) {
					rdbKeyTypesAction.add(rdbkeys[i], rdbkeytypes[i]);
				}
			});
		});
	}

	createEffect(on(selectedItem, () => {
		if(selectedItem().length > 0) {
			selectedKeysAction.add(selectedItem());

			// Add default state
			selectedKeysDataActions.add(selectedItem(), {
				color: "black",
				fill: false,
				key: selectedItem(),
				dedicated_y: false
			});

			setSelectedItem('');
		}
	}));

	createEffect(() => {
		if(displayName().length === 0 || displays.data.has(displayName())) {
			props.validityHandler(false);
		}
		else {
			props.validityHandler(selectedKeys.data.size > 0);
		}
	});

	return (
		<>
			<TextField
				class="grid grid-cols-4 items-center gap-5 my-2"
				onChange={setDisplayName}
				validationState={(displayName().length > 0 && !displays.data.has(displayName())) ? 'valid' : 'invalid'}
			>
				<TextFieldLabel class="text-right">Display Name</TextFieldLabel>
				<TextFieldInput class="col-span-3" type="text"/>
				<TextFieldErrorMessage>
					<Show when={displayName().length === 0}>
						A display name is required.
					</Show>
					<Show when={displays.data.has(displayName())}>
						Display name already in use.
					</Show>
				</TextFieldErrorMessage>
			</TextField>
			<div class="overflow-auto min-h-20 max-h-20 md:max-h-96 border rounded-md border-gray-200 p-5">
				<Show when={selectedKeys.data.size == 0}>
					<div class="place-content-center align-middle w-full flex">
						<div
							class="flex items-center text-center w-full min-h-16 border-4 border-dashed rounded-md h-20 m-2 place-content-center gap-2 text-gray-400 bg-white"
						>
							Select an RDB value below<br/>to add it to this display.
						</div>
					</div>
				</Show>
				<For each={Array.from(selectedKeys.data)}>{(key: string) => {
					return (
						<div class="w-full border border-gray-400 rounded-md shadow-md p-1 mb-5 h-12 hover:h-auto overflow-hidden">
							<div class="m-1">
								<div class="flex place-content-end mb-2">
									<button
										onClick={() => { selectedKeysAction.remove(key); selectedKeysDataActions.remove(key); }}
									>
										<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="size-4"><path d="M18 6l-12 12"></path><path d="M6 6l12 12"></path></svg>
									</button>
								</div>
								<div class="mx-5 -mt-5 justify-center flex font-bold mb-5">
									{key}
								</div>
								<div class="mr-5 mb-5">
									<TextField class="grid grid-cols-4 items-center gap-5 my-2" onChange={(value: string) => {
										const data = selectedKeysData.data.get(key)!;
										data.label = value;
										selectedKeysDataActions.modify(key, data);
									}}>
										<TextFieldLabel class="text-right">Label</TextFieldLabel>
										<TextFieldInput class="col-span-3" placeholder={key} type="text"/>
									</TextField>
									<TextField class="grid grid-cols-4 items-center gap-5 my-2" onChange={(value: string) => {
										const data = selectedKeysData.data.get(key)!;
										data.color = value;
										selectedKeysDataActions.modify(key, data);
									}}>
										<TextFieldLabel class="text-right">Color</TextFieldLabel>
										<TextFieldInput class="col-span-1 w-10 p-1 border-none" type="color"/>
									</TextField>
									<TextField class="grid grid-cols-4 items-center gap-5 my-2">
										<TextFieldLabel class="text-right">Fill</TextFieldLabel>
										<div class="flex items-start space-x-2">
											<Checkbox onClick={() => {
												const display_opts = selectedKeysData.data.get(key)!;
												display_opts.fill = !display_opts.fill;
												selectedKeysDataActions.modify(key, display_opts);
											}} checked={selectedKeysData.data.get(key)!.fill}/>
										</div>
									</TextField>
									<TextField class="grid grid-cols-4 items-center gap-5 my-2">
										<TextFieldLabel class="text-right">Dedicated<br/>Y-Axis</TextFieldLabel>
										<div class="flex items-start space-x-2">
											<Checkbox onClick={() => {
												const display_opts = selectedKeysData.data.get(key)!;
												display_opts.dedicated_y = !display_opts.dedicated_y;
												selectedKeysDataActions.modify(key, display_opts);
											}} checked={selectedKeysData.data.get(key)!.dedicated_y}/>
										</div>
									</TextField>
								</div>
							</div>
						</div>
					);
				}}</For>
			</div>
			<SearchBar
				placeholder="Search for an RDB entry..."
				items={Array.from(rdbKeys.data)}
				// items={['Item1', 'Item2', 'Item3', 'Item4']}
				display={(item: string, select: Function) => {
					const type = MxGenericType.typeFromTypeid(rdbKeyTypes.data.get(item)!);
					let typec = 'flex gap-1 m-1 px-3 border rounded-md bg-white hover:bg-gray-200 shadow-sm active:bg-gray-300 cursor-pointer h-10';
					typec += (type === 'string') ? 'border-rose-500 bg-red-100 text-rose-500 pointer-events-none' : '';
					const typett = (type === 'string') ? 'w-full cursor-default' : 'w-full';
					return (
						<Tooltip>
							<TooltipTrigger class={typett}>
								<div
									class={typec}
									onClick={() => select()}
								>
									<BadgeLabel type={type === 'string' ? 'error' : 'display'} class="px-1 my-2 w-14 place-content-center">{type}</BadgeLabel>
									<div class="place-content-center">{item}</div>
								</div>
							</TooltipTrigger>
							<Show when={(type === 'string')}>
								<TooltipContent class="text-rose-500">
									Cannot create a live display of string variables.
								</TooltipContent>
							</Show>
						</Tooltip>
					);
				}
			}/>
		</>
	);
};

const [openHPM, setOpenHPM] = createSignal<boolean>(false);

const HistoryPlotManager : Component = () => {
	const [valid, setValid] = createSignal<boolean>(false);
	return (
		<div>
			<Dialog open={openHPM()} onOpenChange={setOpenHPM}>
				<DialogTrigger/>
				<DialogContent class="overflow-visible">
					<DialogHeader>
						<DialogTitle class="text-center">Create new history display</DialogTitle>
					</DialogHeader>
					<div class="grid gap-5 py-5">
						<SearchBarProvider>
							<HistoryPlotRDBSearcher validityHandler={setValid}/>
						</SearchBarProvider>
					</div>
					<DialogFooter>
						<MxButton onClick={() => {
							displaysActions.add(displayName(), Array.from(selectedKeysData.data.values()));
							setDisplayName('');
							setOpenHPM(false);
							selectedKeysAction.clear();
							selectedKeysDataActions.clear();
						}} disabled={!valid()}>Done</MxButton>
						<MxButton onClick={() => {
							setDisplayName('');
							setOpenHPM(false);
							selectedKeysAction.clear();
							selectedKeysDataActions.clear();
						}} type="error">Cancel</MxButton>
					</DialogFooter>
				</DialogContent>
			</Dialog>
		</div>
	);
};

const HistoryPlot : Component<{name: string, options: Array<DisplayOptions>}> = (props) => {
	const [isHover, setIsHover] = createSignal<boolean>(false);
	const [data, setData] = createSignal<uPlot.AlignedData>();
	let data_mod : Array<Array<number | null | undefined>>;
	let data_ts : Array<number>;

	onMount(() => {
		data_mod = new Array<Array<number | null | undefined>>(props.options.length);
		data_ts = new Array<number>();
		for(let i = 0; i < data_mod.length; i++) {
			data_mod[i] = new Array<number | null | undefined>();
		}

		// Append the series in realtime/streaming mode
		const rdb = new MxRdb();
		for(let i = 0; i < props.options.length; i++) {
			const key = props.options[i].key;
			const type = MxGenericType.typeFromTypeid(rdbKeyTypes.data.get(key)!);
			rdb.watch(key, (_: string, v: MxGenericType) => {
				const value = Number(v.astype(type));

				// TODO: (Cesar) This timestamp should be moved to the backend event emit
				data_ts.push(Date.now() / 1000);
				data_ts = data_ts.slice(-100);

				data_mod[i].push(value);
				data_mod[i] = data_mod[i].slice(-100); // Limit to 100 for now

				for(let j = 0; j < props.options.length; j++) {
					if(j != i) {
						data_mod[j].push(undefined);
						data_mod[j] = data_mod[j].slice(-100); // Limit to 100 for now
					}
				}

				const alignedData: uPlot.AlignedData = [
					data_ts,
					...data_mod
				];
				setData(alignedData);
			});
		}
	});

	function getFillColor(i: number): string | undefined {
		const fillColorAlpha = 0.3;
		if(props.options[i].fill) {
			const color = cssColorToRGB(props.options[i].color);
			if(color.includes('rgba')) {
				return color.split(',').slice(0, -1).join(',') + `, ${fillColorAlpha})`;
			} else {
				return color.replace(')', `, ${fillColorAlpha})`).replace('rgb', 'rgba');
			}
		}
		return undefined;
	}

	function getSeries() {
		const series = new Array<Object>([{}]);

		for(let i = 0; i < props.options.length; i++) {

			const label = props.options[i].label ? props.options[i].label : props.options[i].key;
			const color = props.options[i].color;

			series.push({
				label: label,
				stroke: color,
				fill: getFillColor(i)
			});
		}
		return series;
	}

	return (
		<div
			onMouseEnter={() => setIsHover(true)}
			onMouseLeave={() => setIsHover(false)}
		>
			<Card title={props.name}>
				<div class="mb-7">
					<RdbPlot options={{
						width: 0,
						height: 0,
						series: getSeries()
					}
					} data={data()} class="h-56"/>
				</div>
				<Show when={isHover()}>
					<div class="flex gap-5">
						<MxButton>Change</MxButton>
						<MxButton type="error">Remove</MxButton>
					</div>
				</Show>
			</Card>
		</div>
	);
};

export const HistoryViewer : Component = () => {
	return (
		<div>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<div class="container">
					<For each={Array.from(displays.data.entries())}>{(item) => {
						return <HistoryPlot name={item[0]} options={item[1]}/>;
					}}</For>
					<div class="w-full flex">
						<button
							class="flex items-center w-full border-4 border-dashed rounded-md h-20 m-2 place-content-center gap-2 text-gray-400 bg-white cursor-pointer hover:bg-gray-100 active:bg-gray-50"
							onClick={() => setOpenHPM(true)}
						>
							<svg width="24" height="24" xmlns="http://www.w3.org/2000/svg" fill-rule="evenodd" fill="gray" clip-rule="evenodd"><path d="M11.5 0c6.347 0 11.5 5.153 11.5 11.5s-5.153 11.5-11.5 11.5-11.5-5.153-11.5-11.5 5.153-11.5 11.5-11.5zm0 1c5.795 0 10.5 4.705 10.5 10.5s-4.705 10.5-10.5 10.5-10.5-4.705-10.5-10.5 4.705-10.5 10.5-10.5zm.5 10h6v1h-6v6h-1v-6h-6v-1h6v-6h1v6z"/></svg>
							New Display
						</button>
					</div>
					<HistoryPlotManager/>
				</div>
			</div>
		</div>
	);
};
