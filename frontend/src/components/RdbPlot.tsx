import { Component, onMount, onCleanup, createSignal, createEffect } from 'solid-js';
import uPlot from 'uplot';

interface PlotInitState {
	options: uPlot.Options;
	data?: uPlot.AlignedData;
	class?: string;
};

export const RdbPlot : Component<PlotInitState> = (props) => {
	let container!: HTMLDivElement;
	let uplot: uPlot;

	const [maxWidth, setMaxWidth] = createSignal<number>(0);
	const [maxHeight, setMaxHeight] = createSignal<number>(0);

	function updateMaxExtents(): void {
		if(container) {
			setMaxWidth(container.getBoundingClientRect().width);
			setMaxHeight(container.getBoundingClientRect().height);
			if(uplot) {
				uplot.setSize({ width: maxWidth(), height: maxHeight() });
			}
		}
	}

	onMount(() => {
		updateMaxExtents();

		const options = props.options;
		options.width = maxWidth();
		options.height = maxHeight();

		uplot = new uPlot(options, props.data, container);

		window.addEventListener('resize', updateMaxExtents);

		createEffect(() => {
			if(uplot && props.data) {
				uplot.setData(props.data);
			}
		});

		onCleanup(() => {
			uplot.destroy();
			window.removeEventListener('resize', updateMaxExtents);
		});
	});

	return (
		<div ref={container} class={props.class}/>
	);
};
