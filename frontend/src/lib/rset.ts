import { createStore } from 'solid-js/store';

export interface SetStoreAction {
	add: Function;
	remove: Function;
	set: Function;
	clear: Function;
};

export function createSetStore<T>(values: Array<T> = []): [{ data: Set<T> }, SetStoreAction] {
	const [store, setStore] = createStore({ data: new Set(values) });

	const add = (value: any) => {
		if(!store.data.has(value)) {
			setStore((p) => { return { data: new Set(p.data).add(value) }; });
		}
	};

	const remove = (value: any) => {
		if(store.data.has(value)) {
			setStore((p) => {
				const n = new Set(p.data);
				n.delete(value);
				return { data: n };
			});
		}
	};

	const set = (values: any) => {
		setStore(() => { return { data: new Set(values)}; });
	};

	const clear = () => {
		setStore(() => { return { data: new Set()}; });
	};

	return [store, { add, remove, set, clear }];
}

