import { Component, onMount, onCleanup, createSignal, createEffect, createMemo } from 'solid-js';
import uPlot from 'uplot';

interface MxPlotProps {
	x: Array<number>;
	y: Array<Array<number>>;
	series: Array<uPlot.Series>;
	scales?: uPlot.Scales;
	axes?: Array<uPlot.Axis>;
	bands?: Array<uPlot.Band>;
	cursor?: uPlot.Cursor;
	class?: string;
};

export const MxGenericPlot : Component<MxPlotProps> = (props) => {
	let container!: HTMLDivElement;
	let uplot: uPlot;

	const [maxWidth, setMaxWidth] = createSignal<number>(0);
	const [maxHeight, setMaxHeight] = createSignal<number>(0);
	const formattedData = createMemo(() => _getDataFormatted(props.x, props.y));

	function _getDataFormatted(x: Array<number>, y: Array<Array<number>>) : uPlot.AlignedData {
		return [
			x,
			...y
		] as uPlot.AlignedData;
	}

	function _updateMaxExtents() : void {
		if(container) {
			setMaxWidth(container.getBoundingClientRect().width);
			setMaxHeight(container.getBoundingClientRect().height);
			if(uplot) {
				uplot.setSize({ width: maxWidth(), height: maxHeight() });
			}
		}
	}

	onMount(() => {
		const awaitContainer = () => {
			if(container) {
				_updateMaxExtents();

				const options = {
					 width: maxWidth(),
					height: maxHeight(),
					series: props.series,
					scales: props.scales,
					  axes: props.axes,
					 bands: props.bands,
					cursor: props.cursor
				};

				uplot = new uPlot(options, formattedData(), container);
				window.addEventListener('resize', _updateMaxExtents);

				createEffect(() => {
					if(uplot) {
						uplot.setData(formattedData());
					}
				});

				onCleanup(() => {
					uplot.destroy();
					window.removeEventListener('resize', _updateMaxExtents);
				});
			}
			else {
				setTimeout(awaitContainer, 10);
			}
		};

		awaitContainer();
	});

	return (
		<div ref={container} class={props.class}/>
	);
};

export const MxHistogramPlot : Component<MxPlotProps> = (props) => {
	const { bars } = uPlot.paths;
	const _full_bars = bars!({size: [1, 10000]});

	onMount(() => {
		props.series = [props.series[0], ...props.series.slice(1).map(x => {x.paths = _full_bars; return x;})];
	});

	return <MxGenericPlot {...props}/>;
};

export const MxScatterPlot : Component<MxPlotProps> = (props) => {

	onMount(() => {
		props.series = [props.series[0], ...props.series.slice(1).map(x => {x.paths = () => null; return x;})];
	});

	return <MxGenericPlot {...props}/>;
};
