import { Component, createSignal, Show, For } from 'solid-js';
import {
	Combobox,
	ComboboxContent,
	ComboboxControl,
	ComboboxInput,
	ComboboxItem,
	ComboboxItemIndicator,
	ComboboxItemLabel,
	ComboboxSection,
	ComboboxTrigger
} from './ui/combobox';

interface ComboSimpleType<T> {
	label: string;
	value: T;
	disabled: boolean;
};

interface ComboSimplePropsType<T> {
	data: Array<ComboSimpleType<T>>;
	default?: string;
	disabled?: boolean;
	placeholder: string;
	onSelect: Function;
};

export const ComboSimple = <T,>(props: ComboSimplePropsType<T>) => {
	const [open, setOpen] = createSignal<boolean>(false);
	const [activeLabel, setActiveLabel] = createSignal<string>('');

	if(props.default) {
		setActiveLabel(props.default);
	}

	return (
		<div>
			<div class="flex">
				<button class="w-20 cursor-pointer disabled:cursor-default disabled:text-gray-500 border border-gray-400 disabled:bg-gray-400 rounded-md text-ellipsis overflow-hidden text-left pl-2"
					onClick={() => setOpen(!open())}
					disabled={props.disabled ?? false}
				>
					{activeLabel() || props.default || props.placeholder}
				</button>
				<Show when={!(props.disabled ?? false)}>
					<svg
						xmlns="http://www.w3.org/2000/svg"
						viewBox="0 0 24 24"
						fill="none"
						stroke="currentColor"
						stroke-width="2"
						stroke-linecap="round"
						stroke-linejoin="round"
						class="size-4 -ml-5 cursor-pointer"
						onClick={() => !props.disabled ? setOpen(!open()) : undefined}
					>
						<path d="M16 15l-4 4l-4 -4" />
					</svg>
				</Show>
				<Show when={props.disabled ?? false}>
					<svg
						xmlns="http://www.w3.org/2000/svg"
						viewBox="0 0 24 24"
						fill="none"
						stroke="currentColor"
						stroke-width="2"
						stroke-linecap="round"
						stroke-linejoin="round"
						class="size-4 -ml-5 cursor-default text-gray-500"
						onClick={() => !props.disabled ? setOpen(!open()) : undefined}
					>
						<path d="M16 15l-4 4l-4 -4" />
					</svg>
				</Show>
			</div>
			<Show when={open()}>
				<div class="absolute z-10 w-20 border border-gray-400 rounded-md bg-gray-100 shadow-md mt-0">
					<For each={props.data}>{(item) =>
						<div
							class="m-1 px-3 border rounded-md bg-gray-100 hover:bg-gray-200 active:bg-gray-300 cursor-pointer"
							onClick={() => { setActiveLabel(item.label); setOpen(false); props.onSelect(item.value); }}
						>{item.label}</div>
					}</For>
				</div>
			</Show>
		</div>
	);
};
