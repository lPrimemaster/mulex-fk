class TNode {
	// [key: string]: TNode | null;
	children: Map<string, TNode>;
	key?: string;

	constructor() {
		this.children = new Map<string, TNode>();
	}
};

export class MxRdbTree {
	private root: TNode;
	
	constructor(keys: Array<string>) {
		this.root = this.build_tree(keys);
	}

	private build_tree(keys: Array<string>) : TNode {
		const root = new TNode();
		for(const key of keys) {
			const subkeys = key.split('/').slice(1); // Keys always start with a slash
			let current = root;

			for(const subkey of subkeys) {
				const children = current.children.get(subkey);
				if(children === undefined) {
					current.children.set(subkey, new TNode());
				}
				current = current.children.get(subkey) as TNode;
			}
			current.key = key;
		}
		return root;
	}

	public print_tree(node: TNode = this.root, indent: string = ''): void {
		const entries = node.children;
		entries.forEach((value, key) => {
			const isLast = (value.key !== undefined);
			const branch = isLast ? '└── ' : '├── ';
			console.log(indent + branch + key, value.key);
			if (value) {
				this.print_tree(value, indent + (isLast ? '    ' : '│   '));
			}
		});
	}
};
