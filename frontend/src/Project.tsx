import { Component, createSignal, For, Show, useContext } from 'solid-js';
import { MxGenericPlot, MxHistogramPlot, MxScatterPlot } from './api/Plot';
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
import { check_condition } from './lib/utils';

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

	async function getPluginJsPath(key: string) {
		const filename = key.split('/').pop()!;
		const path = './../plugins/' + filename.split('.').shift() + '.js';
		return path;
	}

	async function registerPluginFromFile(key: string) {
		const path = await getPluginJsPath(key);
		const plugin = await mxRegisterPluginFromFile(path);
		const plugin_route = '/plugin' + dynamic_plugin_id++;
		addRoute(plugin_route, generatePluginPage(plugin));
		dynamicRoutesActions.add(plugin.id, plugin_route);
	}

	const rdb = new MxRdb();
	rdb.watch('/system/http/plugins/*', (key: string, _: MxGenericType) => {
		registerPluginFromFile(key);
	});

	MxWebsocket.instance.rpc_call('mulex::RdbListSubkeys', [MxGenericType.str512('/system/http/plugins/')], 'generic').then((res) => {
		const keys = res.astype('stringarray');
		for(const key of keys) {
			registerPluginFromFile(key);
		}
	});

	const [x, setx] = createSignal<number>(0);

	setInterval(() => setx(x() + 1), 1000);

	return (
		<div>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				{
				// <div>
				// 	<Card title="plot">
				// 		<MxGenericPlot series={
				// 			[{}, { label: 'MySeries', stroke: 'black', scale: 'K' }, { label: 'other', stroke: 'green', scale: 'K' }]
				// 		}
				// 		x={[1546300800, 1546387200]} y={[[x(), 5], [x() + 1, 1]]} 
				// 		scales={{ x: { time: false }, K: { auto: false, range: [0, 100] }}}
				// 		axes={[{ label: 'Time' }, { scale: 'K', values: (_, t) => t.map(r => r + ' K'), label: 'Temperature'}]}
				// 		class="h-56 w-full mb-5"/>
				// 	</Card>
				// 	<Card title="plot">
				// 		<MxHistogramPlot series={
				// 			[{}, { label: 'MySeries', stroke: 'black', scale: 'K', fill: 'red' }]
				// 		}
				// 		x={[1, 2, 3, 4, 5]} y={[[x(), x() + 1, x() + 2, x() + 3, x() + 4]]}
				// 		scales={{ x: { time: false }, K: { auto: false, range: [0, 100] }}}
				// 		axes={[{ label: 'Time' }, { scale: 'K', values: (_, t) => t.map(r => r + ' K'), label: 'Temperature'}]}
				// 		class="h-56 w-full mb-5"/>
				// 	</Card>
				// 	<Card title="plot">
				// 		<MxScatterPlot series={
				// 			[{}, { label: 'MySeries', stroke: 'black', scale: 'K', fill: 'red' }]
				// 		}
				// 		x={[1, 2, 3, 4, 5]} y={[[x(), x() + 1, x() + 2, x() + 3, x() + 4]]} 
				// 		scales={{ x: { time: false }, K: { auto: false, range: [0, 100] }}}
				// 		axes={[{ label: 'Time' }, { scale: 'K', values: (_, t) => t.map(r => r + ' K'), label: 'Temperature'}]}
				// 		cursor={{ points: { fill: (u, s) => '#0000', size: 12.5}, sync: { key: '' }}}
				// 		class="h-56 w-full mb-5"/>
				// 	</Card>
				// </div>
				}
				<div class="flex gap-5 flex-wrap">
					<Show when={mxGetPluginsAccessor().data.size === 0}>
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
