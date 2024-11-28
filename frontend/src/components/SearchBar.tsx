import { createSignal, createEffect, Component, JSXElement, For, Show } from 'solid-js';
import { BadgeLabel } from './ui/badge-label';

const SearchBarResults: Component<{ matches: Array<string> }> = (props) => {
	return (
		<Show when={props.matches.length > 0}>
			<div class="absolute z-10 border border-gray-400 rounded-md bg-gray-100 shadow-md mt-2 ml-6">
				<For each={props.matches}>{(item) =>
					<div class="m-1 px-3 border rounded-md bg-gray-100 hover:bg-gray-200 active:bg-gray-300 cursor-pointer">{item}</div>
				}</For>
			</div>
		</Show>
	);
};

const SearchBar: Component<{ items: Array<string>, placeholder?: string, children?: JSXElement }> = (props) => {
	const [query, setQuery] = createSignal<string>('');
	const [flen, setFlen] = createSignal<number>(0);

	function handleInputEvent(e: InputEvent) {
		if(e.currentTarget) {
			setQuery((e.currentTarget as HTMLInputElement).value);
		}
	}

	function getFilteredItems(): Array<string> {
		if(query().length === 0) return [];
		return props.items.filter((item) => item.includes(query()));
	}

	createEffect(() => {
		if(query().length > 0) {
			setFlen(getFilteredItems().length);
		}
		else {
			setFlen(0);
		}
	});

	return (
		<>
			<div class="py-5">
				<div class="flex flex-nowrap w-full h-25 rounded-md bg-transparent py-3 text-sm outline-none placeholder:text-muted-foreground disabled:cursor-not-allowed disabled:opacity-50 border border-gray-400">
					<svg
						xmlns="http://www.w3.org/2000/svg"
						viewBox="0 0 24 24"
						fill="none"
						stroke="currentColor"
						stroke-width="2"
						stroke-linecap="round"
						stroke-linejoin="round"
						class="mx-2 size-6 shrink-0 opacity-50 flex-none"
					>
						<path d="M10 10m-7 0a7 7 0 1 0 14 0a7 7 0 1 0 -14 0" />
						<path d="M21 21l-6 -6" />
					</svg>
					<input
						class="bg-transparent outline-none font-medium flex-grow"
						type="text" placeholder={props.placeholder} value={query()} onInput={handleInputEvent}
					/>
					<div class="flex-none mr-5">
						<BadgeLabel class="truncate" type={flen() ? "success" : "error"}>{flen()} Matches</BadgeLabel>
					</div>
				</div>
			<SearchBarResults matches={getFilteredItems()}/>
			</div>
		</>
	);
};

export default SearchBar;
