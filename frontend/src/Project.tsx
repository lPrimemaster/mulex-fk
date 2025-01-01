import { Component, For, Show, useContext } from 'solid-js';
import Sidebar from './components/Sidebar';
import { MxPlugin, mxGetPluginsAccessor } from './lib/plugin';
import { mxRegisterPluginFromFile } from "./lib/plugin";
import { MxRdb } from './lib/rdb';
import { MxGenericType } from './lib/convert';
import { MxWebsocket } from './lib/websocket';
import Card from './components/Card';
import { MxDynamicRouterContext, DynamicRouterContext } from './components/DynamicRouter';
import { createMapStore } from './lib/rmap';
import { A } from '@solidjs/router';

export const Project : Component = () => {
	let dynamic_plugin_id = 0;
	const { addRoute } = useContext(DynamicRouterContext) as MxDynamicRouterContext;
	const [ dynamicRoutes, dynamicRoutesActions ] = createMapStore<string, string>(new Map<string, string>());

	function generatePluginPage(plugin: MxPlugin) {
		const RenderPlug : Component = () => {
			return (
				<div>
					<Sidebar/>
					<div class="p-5 ml-36 mr-auto">
						<plugin.render/>
					</div>
				</div>
			);
		};
		return RenderPlug;
	}

	function getPluginJsPath(key: string) {
		const filename = key.split('/').pop()!;
		const path = './../mxp.comp/mxp.es.' + filename.split('.').shift() + '.js';
		return path;
	}

	async function registerPluginFromFile(path: string) {
		const plugin = await mxRegisterPluginFromFile(path);
		const plugin_route = '/plugin' + dynamic_plugin_id++;
		addRoute(plugin_route, generatePluginPage(plugin));
		dynamicRoutesActions.add(plugin.id, plugin_route);
	}

	// async function registerPlugin(plugin: MxPlugin) {
	// 	mxRegisterPlugin(plugin);
	// 	const plugin_route = '/plugin' + dynamic_plugin_id++;
	// 	addRoute(plugin_route, generatePluginPage(plugin));
	// 	dynamicRoutesActions.add(plugin.id, plugin_route);
	// }

	const rdb = new MxRdb();
	rdb.watch('/system/http/plugins/*', (key: string, _: MxGenericType) => {
		const path = getPluginJsPath(key);
		registerPluginFromFile(path);
	});

	MxWebsocket.instance.rpc_call('mulex::RdbListSubkeys', [MxGenericType.str512('/system/http/plugins/')], 'generic').then((res) => {
		const keys = res.astype('stringarray');
		for(const key of keys) {
			const path = getPluginJsPath(key);
			registerPluginFromFile(path);
		}
	});

	return (
		<div>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<div class="flex gap-5 flex-wrap">
					<Show when={mxGetPluginsAccessor().data.size === 0}>
						<div class="w-full flex">
							<div
								class="items-center w-full border-4 border-dashed rounded-md h-20 m-2 place-content-center gap-2 text-gray-400 bg-white"
							>
								<div class="text-center">No Plugins Found.</div>
								<div class="text-center">Place your plugins under <code>&lt;expCacheDir&gt;/plugins/&lt;myPlugin&gt;.tsx</code></div>
							</div>
						</div>
					</Show>
					<For each={Array.from(mxGetPluginsAccessor().data.values())}>{(Plugin) => {
						return (
							<A href={'/dynamic' + dynamicRoutes.data.get(Plugin.id)!}>
								<Card title="">
									<div class="w-52 h-32 overflow-hidden">
										<div class="text-ellipsis overflow-hidden text-nowrap hover:text-wrap text-center">
											{Plugin.name}
										</div>
										<div class="flex place-content-center">
											<Plugin.icon/>
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
