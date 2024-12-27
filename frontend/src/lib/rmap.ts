import { createStore } from 'solid-js/store';

export interface MapStoreAction {
	add: Function;
	remove: Function;
	modify: Function;
	set: Function;
	clear: Function;
};

export function createMapStore<K, V>(values: Map<K, V>): [{ data: Map<K, V> }, MapStoreAction] {
	const [store, setStore] = createStore({ data: new Map<K, V>(values) });

	const add = (key: K, value: V) => {
		if(!store.data.has(key)) {
			setStore((p) => { return { data: new Map<K, V>(p.data).set(key, value) }; });
		}
	};

	const modify = (key: K, value: V) => {
		if(store.data.has(key)) {
			setStore((p) => { return { data: new Map<K, V>(p.data).set(key, value) }; });
		}
	};

	const remove = (key: K) => {
		if(store.data.has(key)) {
			setStore((p) => {
				const n = new Map<K, V>(p.data);
				n.delete(key);
				return { data: n };
			});
		}
	};

	const set = (values: Map<K, V>) => {
		setStore(() => { return { data: new Map<K, V>(values)}; });
	};

	const clear = () => {
		setStore(() => { return { data: new Map<K, V>()}; });
	};

	return [store, { add, modify, remove, set, clear }];
}

