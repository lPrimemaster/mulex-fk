import {
	createSignal,
	createEffect,
	createContext,
	useContext,
	Component,
	JSXElement,
	For,
	Show,
	Accessor,
	Setter,
    untrack
} from 'solid-js';
import { BadgeLabel } from './ui/badge-label';

export interface SearchBarContextType {
	selectedItem: Accessor<string>;
	setSelectedItem: Setter<string>;
	query: Accessor<string>;
	setQuery: Setter<string>;
	filteredItems: Accessor<Array<string>>;
	setFilteredItems: Setter<Array<string>>;
};

export const SearchBarContext = createContext<SearchBarContextType>();
export const SearchBarProvider: Component<{ children: JSXElement }> = (props) => {
	const [selectedItem, setSelectedItem] = createSignal<string>('');
	const [query, setQuery] = createSignal<string>('');
	const [filteredItems, setFilteredItems] = createSignal<Array<string>>([]);
	return <SearchBarContext.Provider value={{
		selectedItem,
		setSelectedItem,
		query,
		setQuery,
		filteredItems,
		setFilteredItems
	}}>{props.children}</SearchBarContext.Provider>
};

const SearchBarResultsView: Component<{ matches: Array<string>, dropdown: boolean, display: Function | undefined }> = (props) => {
	const { setSelectedItem, setQuery } = useContext(SearchBarContext) as SearchBarContextType;
	return (
		<Show when={props.dropdown && props.matches.length > 0}>
			<div class="absolute z-10 border border-gray-400 rounded-md bg-white shadow-md mt-12 ml-0 overflow-auto w-full">
				<Show when={props.display}>
					<For each={props.matches}>{(item) => {
						const select = () => { setSelectedItem(item); setQuery(''); };
						return (
							<div>{props.display!(item, select)}</div>
						);
					}}</For>
				</Show>
				<Show when={!props.display}>
					<For each={props.matches}>{(item) =>
						<div
							class="m-1 px-3 border rounded-md bg-white hover:bg-gray-200 shadow-sm active:bg-gray-300 cursor-pointer h-10 place-content-center"
							onClick={() => { setSelectedItem(item); setQuery(''); }}
						>{item}</div>
					}</For>
				</Show>
			</div>
		</Show>
	);
};

export const SearchBar: Component<{ items: Array<string>, display?: Function, placeholder?: string, dropdown?: boolean, children?: JSXElement }> = (props) => {
	const { query, setQuery, setFilteredItems } = useContext(SearchBarContext) as SearchBarContextType;
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
			const items = getFilteredItems();
			setFlen(items.length);
			setFilteredItems(items);
		}
		else {
			setFlen(0);
			if(!(props.dropdown ?? true)) {
				setFilteredItems(props.items);
			}
		}
	});

	return (
		<>
			<div class="py-0">
				<div class="flex flex-nowrap w-full rounded-md bg-transparent py-3 text-sm outline-none placeholder:text-muted-foreground disabled:cursor-not-allowed disabled:opacity-50 border border-gray-400">
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
					<Show when={query().length > 0}>
						<div class="flex-none mr-5">
							<BadgeLabel class="truncate" type={flen() ? "success" : "error"}>{flen()} Matches</BadgeLabel>
						</div>
					</Show>
					<SearchBarResultsView matches={getFilteredItems()} dropdown={props.dropdown ?? true} display={props.display}/>
				</div>
			</div>
		</>
	);
};

export const SearchBarServerSide: Component<{ queryFunc: (query: string) => Promise<number>, placeholder?: string }> = (props) => {
	const [query, setQuery] = createSignal<string>('');
	const [flen, setFlen] = createSignal<number>(0);
	const [isWorking, setIsWorking] = createSignal<boolean>(false);

	function handleInputEvent(e: InputEvent) {
		if(e.currentTarget) {
			setQuery((e.currentTarget as HTMLInputElement).value);
		}
	}

	createEffect(() => {
		const qlength = query().length;
		if(qlength > 0 && !untrack(isWorking)) {
			setTimeout(() => {
				if(query().length !== qlength) return;

				setIsWorking(true);
				props.queryFunc(query()).then((count: number) => {
					setIsWorking(false);
					setFlen(count);
				});
			}, 1000);
		}
		else {
			setFlen(0);
		}
	});

	return (
		<div class="py-0">
			<div class="flex flex-nowrap w-full rounded-md bg-transparent py-3 text-sm outline-none placeholder:text-muted-foreground disabled:cursor-not-allowed disabled:opacity-50 border border-gray-400">
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
				<Show when={query().length > 0}>
					<div class="flex-none mr-5">
						<BadgeLabel class="truncate" type={
							isWorking() ? "display" : flen() ? "success" : "error"
						}>
						{
							isWorking() ? 'Working' : `${flen()} Matches`
						}
						</BadgeLabel>
					</div>
				</Show>
			</div>
		</div>
	);
};
