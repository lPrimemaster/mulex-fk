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
	autoYScale?: boolean;
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

				// Y data changes...
				createEffect(() => {
					if(uplot) {
						uplot.setData(formattedData());
						if(props.autoYScale) {
							const getExtents = (scale: string, min: number, max: number) => {
								let emin = min;
								let emax = max;

								if(props.scales && props.scales[scale].range) {
									const range = props.scales[scale].range as uPlot.Range.MinMax;
									if(range[0] !== null) {
										emin = min < (range[0] as number) ? min : (range[0] as number);
									}
								}
								if(props.scales && props.scales[scale]) {
									const range = props.scales[scale].range as uPlot.Range.MinMax;
									if(range[1] !== null) {
										emax = max > (range[1] as number) ? max : (range[1] as number);
									}
								}
								return [emin, emax];
							};

							const flatYData = props.y.flat();
							const [ymin, ymax] = getExtents('y', Math.min(...flatYData), Math.max(...flatYData));
							uplot.setScale('y', { min: ymin, max: ymax });
						}
					}
				});

				// Scale changes
				createEffect(() => {
					const setExtents = (scale: string) => {
						if(props.scales && props.scales[scale] && props.scales[scale].range) {
							const minvalue = (props.scales[scale].range as uPlot.Range.MinMax)[0];
							const maxvalue = (props.scales[scale].range as uPlot.Range.MinMax)[1];
							let min = 0;
							let max = 0;

							if(minvalue !== null) {
								min = minvalue as number;
							}
							if(maxvalue !== null) {
								max = maxvalue as number;
							}
							uplot.setScale(scale, { min: min, max: max });
						}
					};

					setExtents('x');
					setExtents('y');
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
