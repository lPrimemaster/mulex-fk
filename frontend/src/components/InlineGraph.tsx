import { createEffect, createSignal, Component, on } from 'solid-js';

interface MxInlineGraphProps {
	values: Array<number>;
	npoints: number;
	height: number;
	width: number;
	class?: string;
	color: string;
};

export const MxInlineGraph: Component<MxInlineGraphProps> = (props) => {

	function computePath(values: Array<number>) : string {
		if(values.length === 0) {
			return '';
		}

		const h = props.height;
		const w = props.width;
		const n = props.npoints;
		const vmin = Math.min(...values);
		const vmax = Math.max(...values);
		let s = vmax - vmin;
		if(s == 0) {
			s = 1;
		}

		const xs = w / n;
		const ys = h / s;

		return values.map((v, i) => `${i === 0 ? 'M' : 'L'}${i * xs},${h - (v - vmin) * ys/2 - (h/2)}`).join(' ');
	}

	return (
		<svg width={props.width} height={props.height} class={`${props.class || ''} overflow-visible`}>
			<path d={computePath(props.values)} stroke={props.color} fill="none" stroke-width="1.5"/>
		</svg>
	);
};
