import { Component, For, Show, createEffect, createMemo } from 'solid-js';

type DisplayMode = 'percentage' | 'absolute';

interface MxGaugeVerticalProps {
	min: number;
	max: number;
	value: number;
	width: string;
	height: string;
	title: string;
	displayMode?: DisplayMode;
	units?: string;
	class?: string;
	backgroundColor?: string;
};

export const MxGaugeVertical : Component<MxGaugeVerticalProps> = (props) => {
	const percentage = createMemo(() => ((props.value - props.min) / (props.max - props.min)) * 100);

	const style = 'flex flex-col-reverse items-center gap-0.5 h-full overflow-hidden' 
		+ (props.class || '');

	const mode = props.displayMode || 'percentage';

	const segments = [
		{ value:  0, color: '#4caf50' },
		{ value:  5, color: '#4caf50' },
		{ value: 10, color: '#4caf50' },
		{ value: 15, color: '#4caf50' },
		{ value: 20, color: '#4caf50' },
		{ value: 25, color: '#4caf50' },
		{ value: 30, color: '#4caf50' },
		{ value: 35, color: '#4caf50' },
		{ value: 40, color: '#4caf50' },
		{ value: 45, color: '#4caf50' },
		{ value: 50, color: '#4caf50' },
		{ value: 55, color: '#4caf50' },
		{ value: 60, color: '#4caf50' },
		{ value: 65, color: '#4caf50' },
		{ value: 70, color: '#4caf50' },
		{ value: 75, color: '#ff9800' },
		{ value: 80, color: '#ff9800' },
		{ value: 85, color: '#ff9800' },
		{ value: 90, color: '#f44336' },
		{ value: 95, color: '#f44336' }
	];

	return (
		<div class={style} style={{ width: props.width, height: props.height }}>
			<Show when={mode == 'percentage'}>
				<span>{percentage().toFixed(1)}%</span>
			</Show>
			<Show when={mode == 'absolute'}>
				<span>{props.value.toFixed(1)}{props.units || ''} / {props.max.toFixed(1)}{props.units || ''}</span>
			</Show>
			<For each={segments}>{(segment) => {
				return (
					<div style={{
						height: '5%',
						background: segment.color,
						width: '50%',
						opacity: percentage() <= segment.value ? 0.2 : 1.0,
						transition: 'opacity 0.5s ease'
					}} class="rounded-md"/>
				);
			}}</For>
			<span>{props.title}</span>
		</div>
	);
};
