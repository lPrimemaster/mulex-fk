import { Component, Show, createEffect, on } from 'solid-js';
import { Tooltip, TooltipContent, TooltipTrigger } from './components/ui/tooltip';

type MxValuePanelSize = 'small' | 'medium' | 'large' | 'xlarge';

interface MxValueControlProps {
	title: string;
	value: number;
	size: MxValuePanelSize;
	onChange: (value: number) => void;
	increment?: number;
	min?: number;
	max?: number;
	description?: string;
	units?: string;
	class?: string;
};

export const MxValueControl : Component<MxValueControlProps> = (props) => {
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

	const style = size_pad + ' rounded-md shadow-md grid grid-cols-1 bg-blue-300' 
		+ (props.class || '');

	const class_title = size_title;
	const class_value = 'font-bold lining-nums bg-transparent text-center focus:outline-none leading-none cursor-text ' + size_value;

	let ref!: HTMLDivElement;
	let last_input_value: string | null = '';

	createEffect(on(() => props.value, () => {
		if(props.min !== undefined && props.value < props.min) props.onChange(props.min);
		if(props.max !== undefined && props.value > props.max) props.onChange(props.max);
	}));

	createEffect(() => {
		ref.addEventListener('blur', () => {
			const cv = ref.textContent;
			if(!cv) {
				ref.textContent = last_input_value;
				return;
			}

			if(cv !== last_input_value) {
				last_input_value = cv;
				props.onChange(Number(cv));
			}
		});
	});

	return (
		<div class="flex items-center">
			<div class="flex gap-2">
				<Tooltip>
					<TooltipTrigger class="cursor-default">
						<div class={style}>
							<span class={class_title}>{props.title}</span>
							<div class="w-full h-0.5 bg-black"/>
							<div>
								<span contentEditable={true} class={class_value} ref={ref}>{props.value}</span>
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
				<Show when={props.increment}>
					<div class="flex flex-col gap-2">
						<div class={'rounded-md shadow-md px-5 bg-blue-300 h-1/2 cursor-pointer hover:bg-blue-400 active:bg-blue-500 ' + size_title}
							onClick={() => { props.onChange(props.value + props.increment!); last_input_value = props.value.toString(); }}
						>
							<svg width="16px" height="100%" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg">
								<path d="M6 8L2 8L2 6L8 5.24536e-07L14 6L14 8L10 8L10 16L6 16L6 8Z" fill="#000000"/>
							</svg>
						</div>
						<div class={'rounded-md shadow-md px-5 bg-blue-300 h-1/2 cursor-pointer hover:bg-blue-400 active:bg-blue-500 ' + size_title}
							onClick={() => { props.onChange(props.value - props.increment!); last_input_value = props.value.toString(); }}
						>
							<svg class="rotate-180" width="16px" height="100%" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg">
								<path d="M6 8L2 8L2 6L8 5.24536e-07L14 6L14 8L10 8L10 16L6 16L6 8Z" fill="#000000"/>
							</svg>
						</div>
					</div>
				</Show>
			</div>
		</div>
	);
};
