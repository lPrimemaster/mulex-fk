import { JSX, Component, JSXElement, createSignal } from 'solid-js';
// import { createMapStore } from './rmap';

export interface MxPlugin {
	id: string;
	name: string;
	icon: () => JSXElement;
	render: (() => JSX.Element) | Component;
	version: string;
	brief: string;
	description: (() => JSX.Element) | string;
	author: string;
	modified: number;
};

// const [plugins, pluginsActions] = createMapStore<string, MxPlugin>(new Map<string, MxPlugin>());
export const [plugins, setPlugins] = createSignal<Map<string, MxPlugin>>(new Map<string, MxPlugin>());

export function mxRegisterPlugin(plugin: MxPlugin) {
	// pluginsActions.add(plugin.id, plugin);
	setPlugins((p) => new Map<string, MxPlugin>(p).set(plugin.id, plugin));
}

export function mxDeletePlugin(filename: string) {
	// The filename is the id
	// pluginsActions.remove(filename);
	console.log('Removing: ', filename);
	setPlugins((p) => { const np = new Map<string, MxPlugin>(p); np.delete(filename); return np; });
}

export async function mxRegisterPluginFromFile(filename: string, timestamp: number) {
	const module = await import(filename + `?v=${Date.now()}`); // NOTE: (Cesar) Trick to force no caching
	const plugin = {
		id: filename,
		name: module.pname,
		icon: module.icon,
		render: module.render,
		version: module.version,
		brief: module.brief,
		description: module.description,
		author: module.author,
		modified: timestamp
	};

	if(plugin.icon === undefined) {
		plugin.icon = () => <svg fill="#000000" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><path d="M3.111,23H8.667a1,1,0,0,0,1-1V19.9a1.316,1.316,0,0,1,1.009-1.323,1.224,1.224,0,0,1,1.435,1.2V22a1,1,0,0,0,1,1h3.334a2.113,2.113,0,0,0,2.111-2.111V17.444h1.222a3.224,3.224,0,0,0,3.179-3.756A3.314,3.314,0,0,0,19.659,11h-1.1V7.555a2.113,2.113,0,0,0-2.111-2.111H13V4.34a3.313,3.313,0,0,0-2.688-3.3A3.222,3.222,0,0,0,6.556,4.222V5.444H3.111A2.113,2.113,0,0,0,1,7.555V12a1,1,0,0,0,1,1H4.1a1.317,1.317,0,0,1,1.323,1.01,1.223,1.223,0,0,1-1.2,1.434H2a1,1,0,0,0-1,1v4.445A2.113,2.113,0,0,0,3.111,23ZM3,17.444H4.222A3.224,3.224,0,0,0,7.4,13.688,3.313,3.313,0,0,0,4.1,11H3V7.555a.111.111,0,0,1,.111-.111H7.556a1,1,0,0,0,1-1V4.222A1.223,1.223,0,0,1,9.99,3.017,1.316,1.316,0,0,1,11,4.34v2.1a1,1,0,0,0,1,1h4.445a.111.111,0,0,1,.111.111V12a1,1,0,0,0,1,1h2.1a1.317,1.317,0,0,1,1.324,1.01,1.225,1.225,0,0,1-1.205,1.434H17.556a1,1,0,0,0-1,1v4.445a.111.111,0,0,1-.111.111H14.111V19.778A3.222,3.222,0,0,0,10.355,16.6a3.313,3.313,0,0,0-2.688,3.3V21H3.111A.111.111,0,0,1,3,20.889Z"/></svg>;
	}

	mxRegisterPlugin(plugin);
	return plugin;
}

// export function mxGetPluginsAccessor() : { data: Map<string, MxPlugin> } {
// 	return plugins;
// }

