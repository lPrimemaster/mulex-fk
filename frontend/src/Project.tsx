import { Component, For, Show, useContext } from 'solid-js';
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

export const Project : Component = () => {
	const { addRoute, removeRoute } = useContext(DynamicRouterContext) as MxDynamicRouterContext;
	const [ dynamicRoutes, dynamicRoutesActions ] = createMapStore<string, string>(new Map<string, string>());

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

	MxWebsocket.instance.rpc_call('mulex::RdbListSubkeys', [MxGenericType.str512('/system/http/plugins/')], 'generic').then((res) => {
		const keys = res.astype('stringarray');
		for(const key of keys) {
			registerPluginFromFile(key);
		}
	});

	return (
		<div>
			<DynamicTitle title="Project"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<div class="flex gap-5 flex-wrap">
					<Show when={plugins().size === 0}>
						<div class="w-full flex">
							<div
								class="items-center w-full border-4 border-dashed rounded-md h-20 m-2 place-content-center gap-2 text-gray-400 bg-white"
							>
								<div class="text-center">No Plugins Found.</div>
								<div class="text-center">Place your plugins under <code>&lt;expCacheDir&gt;/plugins/&lt;myPlugin&gt;.js</code></div>
								<div class="text-center">Or use `mxplug` to compile.</div>
							</div>
						</div>
					</Show>
					<For each={Array.from(plugins().values())}>{(Plugin) => {
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
