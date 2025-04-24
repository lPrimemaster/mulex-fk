import { Component, createMemo, createSignal, For, onCleanup, onMount, Show, useContext } from 'solid-js';
import Sidebar from './components/Sidebar';
import { MxPlugin, mxRegisterPluginFromFile, mxDeletePlugin, plugins } from './lib/plugin';
import { MxRdb } from './lib/rdb';
import { MxGenericType } from './lib/convert';
import { MxWebsocket } from './lib/websocket';
import Card from './components/Card';
import { MxDynamicRouterContext, DynamicRouterContext } from './components/DynamicRouter';
import { createMapStore } from './lib/rmap';
import { A } from '@solidjs/router';
import { DynamicTitle } from './components/DynamicTitle';
import { MxButton, MxSpinner } from './api';
import { timestamp_tolocaldatetime } from './lib/utils';

export const Project : Component = () => {
	const { updateRoute, removeRoute } = useContext(DynamicRouterContext) as MxDynamicRouterContext;
	const [ dynamicRoutes, dynamicRoutesActions ] = createMapStore<string, string>(new Map<string, string>());
	const [ loadingPlugins, setLoadingPlugins ] = createSignal<boolean>(false);

	function generatePluginPage(plugin: MxPlugin) {
		const RenderPlug : Component = () => {
			const [scroll, setScroll] = createSignal<number>(0);
			const [height, setHeight] = createSignal<number>(0);
			const [showDocs, setShowDocs] = createSignal<boolean>(false);
			const calculateScale = createMemo(() => showDocs() ? 1 : Math.max(1 - scroll() / height(), 0));
			let id: HTMLDivElement | undefined;

			function onScroll() {
				setScroll(window.scrollY);
			}

			onMount(() => {
				setHeight(id!.offsetHeight);

				window.addEventListener('scroll', onScroll);
				onCleanup(() => {
					window.removeEventListener('scroll', onScroll);
				});
			});

			return (
				<div>
					<DynamicTitle title={plugin.name}/>
					<Sidebar/>
					<div class={
						`fixed top-0 left-36 right-0 p-5 border-b-2
						bg-white/50 backdrop-blur-sm z-50
						flex place-items-center place-content-between
						overflow-hidden h-20`
					}
					style={{
						opacity: calculateScale(),
						display: calculateScale() === 0 ? 'none' : 'flex'
					}}
					ref={id}>
						<div class="flex place-items-center">
							<div class="size-10">
								<plugin.icon/>
							</div>
							<div class="ml-5 font-bold">
								{plugin.name}
							</div>
						</div>
						<Show when={!showDocs()}>
							<MxButton disabled={plugin.description === undefined} onClick={() => setShowDocs(true)}>
								Documentation
							</MxButton>
						</Show>
						<Show when={showDocs()}>
							<MxButton disabled={plugin.description === undefined} onClick={() => setShowDocs(false)} type="error">
								Back to plugin
							</MxButton>
						</Show>
					</div>
					<div class="p-5 mt-20 ml-36 mr-auto">
						<Show when={!showDocs()}>
							<plugin.render/>
						</Show>
						<Show when={showDocs()}>
							<plugin.description/>
						</Show>
					</div>
				</div>
			);
		};
		return RenderPlug;
	}

	async function getPluginJsPath(key: string) {
		const filename = key.split('/').pop()!;
		const path = './../plugins/' + filename.split('.').shift() + '.js';
		return path;
	}

	async function getPluginName(key: string) {
		const filename = key.split('/').pop()!;
		return filename.split('.').shift();
	}

	async function registerPluginFromFile(key: string, timestamp: number) {
		const path = await getPluginJsPath(key);
		const plugin = await mxRegisterPluginFromFile(path, timestamp);
		const plugin_route = '/plugin/' + await getPluginName(key);
		updateRoute(plugin_route, generatePluginPage(plugin));
		dynamicRoutesActions.add(plugin.id, plugin_route);
	}

	async function deletePluginFromFile(key: string) {
		const path = await getPluginJsPath(key);
		mxDeletePlugin(path);
		removeRoute('/plugin/' + await getPluginName(key));
		dynamicRoutesActions.remove(path);
	}

	const rdb = new MxRdb();
	rdb.watch('/system/http/plugins/*', (key: string, value: MxGenericType) => {
		registerPluginFromFile(key, Number(value.astype('int64')));
	});

	MxWebsocket.instance.subscribe('mxrdb::keydeleted', (data: Uint8Array) => {
		const key: string = MxGenericType.fromData(data).astype('string');
		if(key.startsWith('/system/http/plugins/')) {
			// A plugin was deleted
			deletePluginFromFile(key);
		}
	});

	setLoadingPlugins(true);
	MxWebsocket.instance.rpc_call('mulex::RdbListSubkeys', [MxGenericType.str512('/system/http/plugins/')], 'generic').then(async (res) => {
		const keys = res.astype('stringarray');
		for(const key of keys) {
			const value = await rdb.read(key);
			await registerPluginFromFile(key, Number(value));
		}
		setLoadingPlugins(false);
	});

	return (
		<div>
			<DynamicTitle title="Project"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<Show when={plugins().size === 0}>
					<div class="w-full flex">
						<div
							class="items-center w-full border-4 border-dashed rounded-md h-20 m-2 place-content-center gap-2 text-gray-400 bg-white"
						>
							<Show when={loadingPlugins()}>
								<MxSpinner description="Loading plugins. Please wait..."/>
							</Show>
							<Show when={!loadingPlugins()}>
								<div class="text-center">No Plugins Found.</div>
								<div class="text-center">Place your plugins under <code>&lt;expCacheDir&gt;/plugins/&lt;myPlugin&gt;.js</code></div>
								<div class="text-center">Or use `mxplug` to compile.</div>
							</Show>
						</div>
					</div>
				</Show>
				<div class="w-full">
					<For each={Array.from(plugins().values())}>{(Plugin) => {
						return (
							<A href={'/dynamic' + dynamicRoutes.data.get(Plugin.id)!}>
								<Card title="">
									<div class="flex h-32 place-content-between">
										<div>
											<div class="w-52 h-32 overflow-hidden">
												<div class="text-ellipsis overflow-hidden text-nowrap hover:text-wrap text-center mb-2">
													{Plugin.name}
												</div>
												<div class="flex place-content-center">
													{
														// Force size to 80x80 px
													}
													<div class="size-20"><Plugin.icon/></div>
												</div>
											</div>
										</div>
										<div class="w-1/2 overflow-scroll">
											<div class="text-center font-bold">Description</div>
											<div class="flex items-center">
												<div class="text-ellipsis text-wrap text-justify">
													{Plugin.brief ?? 'No brief description provided.'}
												</div>
											</div>
										</div>
										<div class="flex place-content-center hidden lg:block">
											<div class="grid grid-cols-2 grid-rows-4 gap-1">
												<span class="font-bold text-right">Filename:&nbsp;</span>
												<span>{Plugin.id.split('/').slice(-1)}</span>

												<span class="font-bold text-right">Author:&nbsp;</span>
												<span>{Plugin.author ?? 'Unspecified'}</span>

												<span class="font-bold text-right">Modified:&nbsp;</span>
												{
													// TODO: (Cesar) Get it from some RPC Call
												}
												<span>{timestamp_tolocaldatetime(Plugin.modified)}</span>

												<span class="font-bold text-right">Version:&nbsp;</span>
												<span>{Plugin.version ?? 'Unspecified'}</span>
											</div>
										</div>
									</div>
								</Card>
							</A>
						);
					}}</For>
				</div>
			</div>
		</div>
	);
};
