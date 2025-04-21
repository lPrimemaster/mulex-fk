import { Component, createSignal, For, Show, useContext } from 'solid-js';
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
import { MxSpinner } from './api';

export const Project : Component = () => {
	const { addRoute, removeRoute } = useContext(DynamicRouterContext) as MxDynamicRouterContext;
	const [ dynamicRoutes, dynamicRoutesActions ] = createMapStore<string, string>(new Map<string, string>());
	const [ loadingPlugins, setLoadingPlugins ] = createSignal<boolean>(false);

	function generatePluginPage(plugin: MxPlugin) {
		const RenderPlug : Component = () => {
			return (
				<div>
					<DynamicTitle title={plugin.name}/>
					<Sidebar/>
					<div class="p-5 ml-36 mr-auto">
						<plugin.render/>
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

	async function registerPluginFromFile(key: string) {
		const path = await getPluginJsPath(key);
		const plugin = await mxRegisterPluginFromFile(path);
		const plugin_route = '/plugin/' + await getPluginName(key);
		addRoute(plugin_route, generatePluginPage(plugin));
		dynamicRoutesActions.add(plugin.id, plugin_route);
	}

	async function deletePluginFromFile(key: string) {
		const path = await getPluginJsPath(key);
		mxDeletePlugin(path);
		removeRoute('/plugin/' + await getPluginName(key));
		dynamicRoutesActions.remove(path);
	}

	const rdb = new MxRdb();
	rdb.watch('/system/http/plugins/*', (key: string, _: MxGenericType) => {
		registerPluginFromFile(key);
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
			await registerPluginFromFile(key);
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
												<div class="text-ellipsis overflow-hidden text-nowrap hover:text-wrap text-center">
													{Plugin.name}
												</div>
												<div class="flex place-content-center">
													<Plugin.icon/>
												</div>
											</div>
										</div>
										<div class="w-1/2 overflow-scroll">
											<div class="text-center font-bold">Description</div>
											<div class="flex items-center">
												<div class="text-ellipsis text-wrap text-justify">
													{Plugin.description ?? 'No description provided.'}
												</div>
											</div>
										</div>
										<div class="flex place-content-center hidden lg:block">
											<div class="grid grid-cols-1 grid-rows-4 gap-1">
												<div>
													<span class="font-bold">Filename:&nbsp;</span>
													<span>{Plugin.id.split('/').slice(-1)}</span>
												</div>

												<div>
													<span class="font-bold">Author:&nbsp;</span>
													<span>{Plugin.author ?? 'Unspecified'}</span>
												</div>

												<div>
													<span class="font-bold">Modified:&nbsp;</span>
													{
														// TODO: (Cesar) Get it from some RPC Call
													}
													<span>03-01-2025 02:07:23PM</span>
												</div>

												<div>
													<span class="font-bold">Version:&nbsp;</span>
													<span>{Plugin.version ?? 'Unspecified'}</span>
												</div>
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
