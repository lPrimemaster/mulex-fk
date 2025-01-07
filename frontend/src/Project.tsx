import { Component, For, Show, createSignal, useContext } from 'solid-js';
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
	let transpiler_on: boolean | undefined = undefined;

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

	MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512('/system/http/online_transpiler')], 'generic').then((res) => {
		transpiler_on = res.astype('bool');
	});

	async function getPluginJsPath(key: string) {
		await check_condition(() => transpiler_on !== undefined);

		if(transpiler_on) {
			const filename = key.split('/').pop()!;
			const path = './../mxp.comp/mxp.es.' + filename.split('.').shift() + '.js';
			return path;
		}
		else {
			const filename = key.split('/').pop()!;
			const path = './../plugins/' + filename.split('.').shift() + '.js';
			return path;
		}
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

	const [v, sv] = createSignal(0);
	const [v0, sv0] = createSignal(0);
	const [c, sc] = createSignal('red');
	const [b, sb] = createSignal(false);

	setInterval(() => {sv(v() + 1)}, 1000);
	// setInterval(() => sc(c() === 'red' ? 'green' : 'red'), 2000);

	return (
		<div>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
			{/*<div class="flex gap-5 mb-5">
					<MxValuePanel title="C1" value={3.1415926535} size="xlarge" units="rad"/>
					<MxValuePanel title="Temperature C1" color={c()} value={v()} size="xlarge" units="&deg;C" reactive/>
					<MxValueControl title="C1 setpoint" min={0} value={v0()} description="my setpoint" size="xlarge" increment={0.1} units="Bq" onChange={(val) => {sv0(val)}}/>
					<MxButton class="font-bold text-4xl">Spin</MxButton>
					<MxButton type="error" class="font-bold text-4xl">Stop</MxButton>
					<MxSwitch label="Lock" value={b()} onChange={sb}/>
				</div>
				<div class="flex gap-5">
					<MxGaugeVertical min={0} max={100} value={50} width="128px" height="200px" title="Gauge"/>
					<MxGaugeVertical min={0} max={2000} value={1036} width="200px" height="200px" title="Pressure" units="mbar" displayMode="absolute"/>
					<MxSelector title="Operation Mode" value="Silent" size="xlarge" onSelect={(v) => { console.log(v) }} options={['Silent', 'Loud', 'XtraLoud']}/>
				</div>*/}
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
