import { JSX, createSignal, Accessor, Component } from 'solid-js';

export interface MxPlugin {
	id: string;
	render: (() => JSX.Element) | Component;
};

const [plugins, setPlugins] = createSignal<Array<MxPlugin>>(new Array<MxPlugin>());

export function mxRegisterPlugin(plugin: MxPlugin) {
	setPlugins((p) => [...p, plugin]);
}

export async function mxRegisterPluginFromFile(filename: string) {
	const module = await import(filename);
	mxRegisterPlugin({ id: filename, render: module.default });
}

export function mxGetPluginsAccessor() : Accessor<Array<MxPlugin>> {
	return plugins;
}

