import { Component, createSignal } from 'solid-js';
import Sidebar from './components/Sidebar';
import { DynamicTitle } from './components/DynamicTitle';
import { DiGraph } from './components/DiGraph';

export const EventsViewer : Component = () => {

	const [x, sx] = createSignal(0);
	
	setInterval(() => {sx(x() + 1)}, 1000);

	return (
		<div>
			<DynamicTitle title="Events"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto min-h-dvh flex flex-col">
				<DiGraph
					nodes={[
						{
							x: 100, y: 100, title: 'Node 1', id: 'n0', content: <div class="text-wrap">Hello content! With wrapping enabled, since it is large.</div>
						},
						{
							x: 300, y: 150, title: 'Node 2', id: 'n1', content: <div>Hello content!</div>
						},
						{
							x: 300, y: 300, title: 'Node 3', id: 'n2', content: <div>Hello content!</div>
						}
					]}
					edges={[{ from: 'n0', to: 'n1', label: x() + ' Gb/s' }, { from: 'n0', to: 'n2' }]}
				/>
			</div>
		</div>
	);
};
