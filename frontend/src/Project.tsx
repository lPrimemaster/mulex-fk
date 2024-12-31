import { Component, For } from 'solid-js';
import Sidebar from './components/Sidebar';
import { mxGetPluginsAccessor } from './lib/plugin';
import { mxRegisterPluginFromFile } from "./lib/plugin";

const filename = './../mxp.comp/mxp.es.testp.js';
mxRegisterPluginFromFile(filename);

export const Project : Component = () => {
	return (
		<div>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<For each={mxGetPluginsAccessor()()}>{(Plugin) => {
					return <Plugin.render/>
				}}</For>
			</div>
		</div>
	);
};
