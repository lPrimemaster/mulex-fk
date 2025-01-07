import { Component, Show, createEffect, createSignal, on } from 'solid-js';
import { Tooltip, TooltipContent, TooltipTrigger } from '../components/ui/tooltip';

type MxValuePanelSize = 'small' | 'medium' | 'large' | 'xlarge';

interface MxValuePanelProps {
	title: string;
	value: string | number;
	size: MxValuePanelSize;
	color?: string;
	reactive?: boolean;
	description?: string;
	units?: string;
	class?: string;
};

export const MxValuePanel : Component<MxValuePanelProps> = (props) => {
	const DEF_COLOR = props.color || '#93c5fd'; // bg-blue-300
	const REC_COLOR = '#fef9c3'; 				// bg-yellow-100

	const [transition, setTransition] = createSignal<string>('');
	const [bgColor, setBgColor] = createSignal<string>(DEF_COLOR);

	let size_title = '';
	let size_value = '';
	let size_pad = '';
	let units_style = '';

	if(props.size === 'small') {
		size_title = 'text-sm';
		size_value = 'text-base';
		size_pad = 'p-1 gap-0';
		units_style = 'pl-0 text-xs align-top';
	}
	else if(props.size === 'medium') {
		size_title = 'text-lg';
		size_value = 'text-xl';
		size_pad = 'p-2 gap-1';
		units_style = 'pl-0 text-sm align-top';
	}
	else if(props.size === 'large') {
		size_title = 'text-2xl';
		size_value = 'text-3xl';
		size_pad = 'p-4 gap-2';
		units_style = 'pl-0 text-lg align-top';
	}
	else if(props.size === 'xlarge') {
		size_title = 'text-4xl';
		size_value = 'text-5xl';
		size_pad = 'p-5 gap-2';
		units_style = 'pl-1 text-2xl align-top';
	}

	const style = size_pad + ' rounded-md shadow-md grid grid-cols-1' 
		+ (props.class || '');

	const class_title = size_title;
	const class_value = 'overflow-hidden text-ellipsis font-bold lining-nums ' + size_value;

	createEffect(on(
		() => props.value,
		() => {
			if(props.reactive) {
				setBgColor(REC_COLOR);
				setTransition('none');
				setTimeout(() => {
					setBgColor(props.color || DEF_COLOR);
					setTransition('background-color 0.75s');
				}, 100);
			}
		}
	));

	createEffect(on(
		() => props.color,
		() => {
			if(props.color) {
				setBgColor(props.color);
			}
		}
	));

	return (
		<div class="flex items-center">
			<Tooltip>
				<TooltipTrigger class="cursor-default">
					<div class={style} style={{
						"background-color": bgColor(),
						transition: transition()
					}}
					>
						<span class={class_title}>{props.title}</span>
						<div class="w-full h-0.5 bg-black"/>
						<div>
							<span class={class_value}>{props.value}</span>
							<span class={units_style}>{props.units || ''}</span>
						</div>
					</div>
				</TooltipTrigger>
				<Show when={props.description}>
					<TooltipContent>
						{props.description}
					</TooltipContent>
				</Show>
			</Tooltip>
		</div>
	);
};
