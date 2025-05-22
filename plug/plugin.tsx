import { Component, createSignal, JSXElement, onCleanup, onMount } from 'solid-js';
import apperture from '../../assets/aperture.svg';
import { MxButton, MxGaugeVertical, MxSelector, MxSpinner, MxSwitch, MxValueControl, MxValuePanel } from '~/api';
import { MxDoubleSwitch } from '~/api/Switch';
import { MxGenericPlot, MxHistogramPlot } from '~/api/Plot';
import { MxRpc } from '~/lib/rpc';
import { MxGenericType } from '~/lib/convert';
import { MxRdb } from '~/lib/rdb';
import { MxEvent } from '~/lib/event';
import { MxWebsocket } from '~/lib/websocket';

// You can write any ammount of components you wish
// In reallity the plugin.tsx main entrypoint
// lives at 'src/entry/'
// all the other 'src/' locations are free to use
// there are no real limitations
const Detail : Component<{ children: JSXElement, class?: string }> = (props) => {
	return (
		<div class={`flex w-1/2 border shadow-md bg-red-100 rounded-md place-items-center ${props.class ?? ''}`}>
			<img class="size-4 ml-5" src={apperture}/>
			<div class="ml-2">
				{props.children}
			</div>
		</div>
	);
};

// A solid-js component
// Everything solid-js related is valid on this context
const MyPlugin : Component<{ cleanup: (cb: () => void) => void }> = (props) => {
	const [value, setValue] = createSignal<number>(0);
	const [control, setControl] = createSignal<number>(0);
	const [nukes, setNukes] = createSignal<boolean>(false);
	const [fruit, setFruit] = createSignal<string>('banana');
	const [random, setRandom] = createSignal<number>(0);
	const [histRandom, setHistRandom] = createSignal<Array<number>>([]);
	const HIST_SIZE = 1000;

	onMount(() => {
		const id = setInterval(() => setValue(value() + 1), 1000);
		const id2 = setInterval(() => setRandom(Math.random()), 1000);
		const id3 = setInterval(() => setHistRandom(Array.from({ length: HIST_SIZE }, () => Math.ceil(Math.random() * 10))), 1000);

		// solidjs onCleanup does not work
		// these are the limitations of precompiled js served files
		props.cleanup(() => {
			clearInterval(id);
			clearInterval(id2);
			clearInterval(id3);

			// This is usually recommended
			// Otherwise any connection to the server will keep alive
			// This is typically not what is wanted. However, if you
			// wish to keep some behaviour running in the background
			// even after the plugin page is closed,
			// comment the following line
			MxWebsocket.instance.close();
		});
	});

	return (
		<div class="container">
			<p class="mb-5">
				The mxfk sample plugin gives details on how to use the frontend API features from within
				a typescript/javascript context.
				Here are some of the features available directly as part of the API.
			</p>

			<Detail>Value panels</Detail>
			<div class="flex gap-5 py-5">
				<MxValuePanel title="Title" value={42} size="small"/>
				<MxValuePanel title="Title" value={42} size="medium"/>
				<MxValuePanel title="Title" value={42} size="large"/>
				<MxValuePanel title="Title" value={42} size="xlarge"/>
				<p class="text-wrap w-1/2 text-justify">
					Value panels feature small, medium, large and xlarge sizes respectively.
					Values and title can programatically change when required.
					To the right is the example of a reactive <code>MxValuePanel</code> with changing value.
				</p>
				<MxValuePanel title="Reactive" value={value()} size="xlarge" reactive/>
			</div>

			<Detail>Value Control</Detail>
			<div class="flex gap-5 py-5">
				<MxValueControl title="Control" value={control()} size="small" onChange={setControl}/>
				<MxValueControl title="Control" value={control()} size="medium" onChange={setControl}/>
				<MxValueControl title="Control" value={control()} size="large" onChange={setControl}/>
				<MxValueControl title="Control" value={control()} size="xlarge" onChange={setControl}/>
				<p class="text-wrap w-1/2 text-justify">
					Control panels feature small, medium, large and xlarge sizes respectively.
					Values and title can programatically change when required.
					Control panels can have changing units, +/- buttons, and user limits.
				</p>
				<MxValueControl
					title="Control"
					value={control()}
					size="xlarge"
					onChange={setControl}
					increment={1}
					units="rad"
					min={0}
					max={10}
					description="Some angle"
				/>
			</div>

			<Detail>Other controls</Detail>
			<div class="grid grid-cols-2 grid-rows-2">
				<div>
					<Detail class="bg-yellow-100 ml-5 mt-5 w-1/4">Buttons</Detail>
					<div class="flex gap-5 py-5 ml-5">
						<div class="grid gap-2 grid-cols-2 grid-rows-2">
							<MxButton onClick={() => {}}>A button that does nothing</MxButton>
							<MxButton onClick={() => {}} type="error">A red button that does nothing</MxButton>
							<MxButton onClick={() => {}} disabled>A disabled button</MxButton>
						</div>
					</div>
				</div>

				<div>
					<Detail class="bg-yellow-100 ml-5 mt-5 w-1/4">Switches</Detail>
					<div class="flex gap-5 py-5 ml-5">
						<div class="grid gap-5 grid-cols-2 grid-rows-2">
							<MxSwitch label="Activate nukes" value={nukes()} onChange={setNukes}/>
							<MxSwitch label="Disabled" value={nukes()} onChange={setNukes} disabled/>
							<MxDoubleSwitch labelFalse="I am off" labelTrue="I am on" value={nukes()} onChange={setNukes}/>
							<MxDoubleSwitch labelFalse="I am off" labelTrue="I am on" value={nukes()} onChange={setNukes} disabled/>
						</div>
					</div>
				</div>

				<div class="col-span-2">
					<Detail class="bg-yellow-100 ml-5 mt-5 w-1/4">Selectors</Detail>
					<div class="flex gap-5 py-5 ml-5">
						<MxSelector
							title="Fruits" value={fruit()} onSelect={setFruit} size="small"
							options={['banana', 'orange', 'coconut', 'avocado', 'strawberry']}
							description="Favourite fruit."
						/>
						<MxSelector
							title="Fruits" value={fruit()} onSelect={setFruit} size="medium"
							options={['banana', 'orange', 'coconut', 'avocado', 'strawberry']}
							description="Favourite fruit."
						/>
						<MxSelector
							title="Fruits" value={fruit()} onSelect={setFruit} size="large"
							options={['banana', 'orange', 'coconut', 'avocado', 'strawberry']}
							description="Favourite fruit."
						/>
						<MxSelector
							title="Fruits" value={fruit()} onSelect={setFruit} size="xlarge"
							options={['banana', 'orange', 'coconut', 'avocado', 'strawberry']}
							description="Favourite fruit."
						/>
						<p class="text-wrap w-1/4 text-justify">
							Selectors allow you to change values out of multiple option.
							Basically they are just a simple combobox replica.
						</p>
					</div>
				</div>
			</div>
			<Detail class="bg-yellow-100 ml-5 mt-5 w-1/4">Spinner</Detail>
			<div class="flex gap-5 py-5 ml-5">
				<div class="grid gap-2 grid-cols-2 grid-rows-2">
					<MxSpinner description="Waiting for something"/>
				</div>
			</div>

			<Detail>Gauges</Detail>
			<div class="flex gap-5 py-5">
				<MxGaugeVertical min={10} max={50} value={random() * 40 + 10} width="200px" height="300px" title="Random value" displayMode="absolute"/>
				<MxGaugeVertical min={10} max={50} value={random() * 40 + 10} width="200px" height="300px" title="Random value pct" displayMode="percentage"/>
				<MxGaugeVertical min={10} max={50} value={42} width="200px" height="300px" title="Disk usage" displayMode="absolute"
					units="PB"
				/>
				<p class="text-wrap w-1/4 text-justify">
					Gauges give you a way to graphically visualize data.
					Percentage and absolute modes are available. Vertical gauges only.
				</p>
			</div>

			<Detail>Plots</Detail>
			<div class="flex gap-5 py-5">
				<MxHistogramPlot
					series={[{}, { label: 'Data 1', stroke: 'black' }, { label: 'Data 2', stroke: 'red' }]}
					x={Array.from({ length: HIST_SIZE }, (_, i) => i + 1)}
					y={[Array.from(histRandom(), (v, _) => v * 10), histRandom()]}
					scales={{ x: { time: false }, y: { range: [0, 100] }}}
					class="h-56 w-3/4"
					// autoYScale
				/>
				<p class="text-wrap w-1/4 text-justify">
					<code>MxHistogramPlot</code> plots as the name indicates can show you histograms in real time.
					It uses the 'uplot' package for performance.
					One can add multiple series if required.
					Auto y scale is also available to conform the data.
				</p>
			</div>
			<div class="flex gap-5 py-5">
				<MxGenericPlot
					series={[{}, { label: 'Data 1', stroke: 'blue' }, { label: 'Data 2', stroke: 'red' }]}
					x={Array.from({ length: 100 }, (_, i) => i + 1)}
					y={[Array.from({ length: 100 }, (_, i) => Math.exp(i * 0.02)), Array.from({ length: 100 }, (_, i) => Math.exp(-(i - 50) * 0.04))]}
					scales={{ x: { time: false }, y: { range: [0, 10] }}}
					class="h-56 w-3/4"
					// autoYScale
				/>
				<p class="text-wrap w-1/4 text-justify">
					<code>MxGenericPlot</code> plots as the name indicates can show you any type of plot in real time.
					It uses the 'uplot' package for performance and is basically a wrapper around the uplot object.
					You can look under the uplot package documentation for more details about this plot. Or alternatively,
					inspect the <code>src/api/Plot.tsx</code> file.
				</p>
			</div>

			<Detail class="mt-5">Communication features</Detail>
			<p class="text-wrap w-full text-justify mt-5">
				This section features examples of server communication via the multiple options available.
				For a more detailed description visit the wiki page regarding frontend development 
				<a href="https://lprimemaster.github.io/mulex-fk/" style={{
					"text-decoration": "underline",
					"color": "blue"
				}} target="_blank">here</a>.
			</p>

			<Detail class="bg-yellow-100 ml-5 mt-5 w-1/4">RPC</Detail>
			<div class="flex gap-5 py-5">
				{(() => {
					const [ename, setEname] = createSignal<string>('');
					return (
						<>
							<MxButton onClick={ async () => {
								const handle = await MxRpc.Create();
								const name: MxGenericType = await handle.SysGetExperimentName();
								const expname: string = name.astype('string');
								setEname(expname);
							}}>Call <code>mulex::SysGetExperimentName</code></MxButton>
							<MxValuePanel title="Experiment name" value={ename()} size="xlarge"/>
						</>
					);
				})()}
				<div class="text-wrap w-1/2 flex place-items-center">
					<div class="border rounded-md p-5">
						<code>const handle = await MxRpc.Create();</code><br/>
						<code>const name: MxGenericType = await handle.SysGetExperimentName();</code><br/>
						<code>const expname: string = name.astype('string');</code><br/>
					</div>
				</div>
			</div>

			<Detail class="bg-yellow-100 ml-5 mt-5 w-1/4">RDB</Detail>
			<div class="flex gap-5 py-5">
				{(() => {
					const [cpu, setCpu] = createSignal<number>(0);
					return (
						<>
							<MxButton onClick={ async () => {
								const handle = new MxRdb();
								const cpu_usage: number = await handle.read('/system/metrics/cpu_usage');
								setCpu(cpu_usage);
							}}>Fetch <code>/system/metrics/cpu_usage</code></MxButton>
							<MxValuePanel title="Cpu Usage" value={cpu().toPrecision(2)} size="xlarge"/>
						</>
					);
				})()}
				<div class="text-wrap flex place-items-center">
					<div class="border rounded-md p-5">
						<code>const handle = new MxRdb();</code><br/>
						<code>const cpu_usage: number = await handle.read('/system/metrics/cpu_usage');</code><br/>
					</div>
				</div>
			</div>
			<div class="flex gap-5 py-5">
				{(() => {
					const [cpu, setCpu] = createSignal<number>(0);

					const handle = new MxRdb();
					handle.watch('/system/metrics/cpu_usage', (_: string, v: MxGenericType) => {
						const cpu_usage: number = v.astype('float64');
						setCpu(cpu_usage);
					});

					return (
						<>
							<div class="border rounded-md p-5 place-content-center">
								Continuous watch
							</div>
							<MxValuePanel title="Cpu Usage" value={cpu().toPrecision(2)} size="xlarge"/>
						</>
					);
				})()}
				<div class="text-wrap flex place-items-center">
					<div class="border rounded-md p-5">
						<code>const handle = new MxRdb();</code><br/>
						<code>
							handle.watch(<br/>
							&nbsp;'/system/metrics/cpu_usage',<br/>
							&nbsp;(_: string, v: MxGenericType) =&gt; setCpu(v.astype('float64'))<br/>
							);
						</code><br/>
					</div>
				</div>
			</div>

			<Detail class="bg-yellow-100 ml-5 mt-5 w-1/4">Events</Detail>
			<div>
				{(() => {
					const [uvar, setUvar] = createSignal<BigInt>(BigInt(0));
					const [ukey, setUkey] = createSignal<string>('');

					MxWebsocket.instance.subscribe('mxevt::rdbw-6c83cf8ccd1e32e6', (data: Uint8Array) => {
						const key = MxGenericType.fromData(data).astype('string');
						const value = MxGenericType.fromData(data.subarray(512), 'generic').astype('uint64');
						setUvar(value);
						setUkey(key);
					});

					return (
						<>
							<div class="flex gap-5 py-5">
								<div class="border rounded-md p-5 place-content-center">
										Event subscription
								</div>
								<div class="text-wrap flex place-items-center">
									<div class="border rounded-md p-5">
										Simulating event reading via RDB watch events (which use built-in events).
									</div>
								</div>
							</div>
							<div class="flex gap-5 py-5">
								<MxValuePanel title="Key" value={ukey()} size="xlarge"/>
								<MxValuePanel title="Value" value={uvar().toString()} size="xlarge"/>
							</div>
						</>
					);
				})()}
			</div>

			<Detail class="mt-5">Summary</Detail>
			<p class="text-wrap w-full text-justify mt-5">
				The plugin frontend is a versatile tool to write GUI's to manage/control your backend(s).<br/>
				A feature that is missing in this demo plugin is the user RPC call, which allows one to call
				functions defined in one's backend binary. To know more about this visit the documentation page.<br/>
			</p>
			<div class="mt-5">
				In summary here are a few tips on how to use the mechanisms showcased above:

				<div class="bg-yellow-100 border rounded-md px-5 py-1 flex place-items-center mt-5 ml-5">
					<img class="size-4 mr-2" src={apperture}/>
					For slow/medium evolving variables (let's say &lt;10Hz) use RDB variables to share data.<br/>
					These are also usefull when one want to change values without the need to build a custom frontend.
				</div>
				<div class="bg-yellow-100 border rounded-md px-5 py-1 flex place-items-center mt-5 ml-5">
					<img class="size-4 mr-2" src={apperture}/>
					For fast evolving variables use events to share data.<br/>
					Events can be registered on a backend.<br/>
					Unlike RDB variables, these are highly pipelinable.
				</div>
				<div class="bg-yellow-100 border rounded-md px-5 py-1 flex place-items-center mt-5 ml-5">
					<img class="size-4 mr-2" src={apperture}/>
					Displaying data is done with built-in value panels and generic plots.<br/>
					However, one could use it's one code/libraries to do this as needed/wanted.
				</div>
				<div class="bg-yellow-100 border rounded-md px-5 py-1 flex place-items-center mt-5 ml-5">
					<img class="size-4 mr-2" src={apperture}/>
					To aid in development 'mxplug' contains a hotswap mode that rebuilds everytime a source file changes.<br/>
					Try running this plugin with 'mxplug --build &lt;expname&gt; --hotswap' and change this source file to see changes in realtime.
				</div>
			</div>
		</div>
	);
};

// MANDATORY: Name seen by the mx server instance
export const pname = 'MyPlugin';

// MANDATORY: The main render component
export const render = MyPlugin;

// MANDATORY: The current plugin version (this comes from package.json)
// 			  However one could overwrite this to some other value if wanted
export const version = __APP_VERSION__;

//  OPTIONAL: Small description text to display inline at the plugins project page
export const brief = 'The mxfk default sample plugin. Displays basic functionality from the mxfk typescript API.';

//  OPTIONAL: Documentation style description text to help/give insight on how to use this plugin
//  		  Can be a string or a JSX Element
export const description = () => {
	return (
		<>
			<p>The mxfk default sample plugin. Displays basic functionality from the mxfk typescript API.</p>
			<p>
				Details the user on how to use some of the mxfk typescript API by giving simple examples.
				This plugin is intended to work standalone and compile via the <code>mxplug --build &lt;expname&gt;</code> command.
			</p>
		</>
	);
};

//  OPTIONAL: Author name to display at the plugins project page
export const author = 'César Godinho';

//  OPTIONAL: Custom icon to display at the plugins project page
//  		  Preferred size 80px square image
export const icon = () => <img src={apperture}/>;
