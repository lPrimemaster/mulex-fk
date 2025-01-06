import { Component, For, Show, createSignal } from 'solid-js';
import { Tooltip, TooltipContent, TooltipTrigger } from './components/ui/tooltip';

type MxSelectorSize = 'small' | 'medium' | 'large' | 'xlarge';

interface MxSelectorProps {
	title: string;
	value: string;
	size: MxSelectorSize;
	onSelect: (value: string) => void;
	options: Array<string>;
	description?: string;
	class?: string;
};

export const MxSelector : Component<MxSelectorProps> = (props) => {
	const [activeItem, setActiveItem] = createSignal<string>(props.value);
	const [open, setOpen] = createSignal<boolean>(false);

	let size_title = '';
	let size_value = '';
	let size_pad = '';
	let units_style = '';
	let dd_offset = '';
	let arrow_size = '';
	let arrow_offset = '';

	if(props.size === 'small') {
		size_title = 'text-sm';
		size_value = 'text-base';
		size_pad = 'p-1 gap-1';
		units_style = 'pl-0 text-xs align-top';
		dd_offset = 'ml-1';
		arrow_size = '16px';
		arrow_offset = '';
	}
	else if(props.size === 'medium') {
		size_title = 'text-lg';
		size_value = 'text-xl';
		size_pad = 'p-2 gap-1';
		units_style = 'pl-0 text-sm align-top';
		dd_offset = 'ml-2 -mt-1';
		arrow_size = '22px';
		arrow_offset = '';
	}
	else if(props.size === 'large') {
		size_title = 'text-2xl';
		size_value = 'text-3xl';
		size_pad = 'p-4 gap-2';
		units_style = 'pl-0 text-lg align-top';
		dd_offset = 'ml-4 -mt-3';
		arrow_size = '26px';
		arrow_offset = '';
	}
	else if(props.size === 'xlarge') {
		size_title = 'text-4xl';
		size_value = 'text-5xl';
		size_pad = 'p-5 gap-2';
		units_style = 'pl-1 text-2xl align-top';
		dd_offset = 'ml-5 -mt-4';
		arrow_size = '32px';
		arrow_offset = '-mb-2';
	}

	const style = size_pad + ' rounded-md shadow-md grid grid-cols-1 bg-blue-300' 
		+ (props.class || '');

	const class_title = size_title;
	const class_value = 'font-bold bg-transparent text-center mx-auto ' + size_value;

	let pref!: HTMLDivElement;

	return (
		<div class="flex items-center">
			<div class="flex gap-2">
				<Tooltip>
					<TooltipTrigger class="cursor-default">
						<div class={style}>
							<span class={class_title}>{props.title}</span>
							<div class="w-full h-0.5 bg-black"/>
							<div class="flex border-2 border-black rounded-md place-content-center gap-3 cursor-pointer"
								onClick={() => {setOpen(!open())}}
								ref={pref}
							>
								<span class={class_value}>{activeItem()}</span>
								<div class={'p-1 ml-auto ' + arrow_offset}>
									<svg class="rotate-180" width={arrow_size} height="100%" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg">
										<path d="M6 8L2 8L2 6L8 5.24536e-07L14 6L14 8L10 8L10 16L6 16L6 8Z" fill="#000000"/>
									</svg>
								</div>
							</div>
						</div>
						<Show when={open()}>
							<div class={'absolute z-10 border border-black rounded-md bg-blue-300 shadow-md mx-auto ' + dd_offset} style={{
								width: `${pref.offsetWidth}px`
							}}>
								<For each={props.options}>{(item) =>
									<div
										class="m-1 px-3 rounded-sm text-left hover:bg-blue-400 active:bg-blue-500 cursor-pointer"
										onClick={() => { setActiveItem(item); setOpen(false); props.onSelect(item); }}
									>{item}</div>
								}</For>
							</div>
						</Show>
					</TooltipTrigger>
					<Show when={props.description}>
						<TooltipContent>
							{props.description}
						</TooltipContent>
					</Show>
				</Tooltip>
			</div>
		</div>
	);
};
