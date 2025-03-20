import { Component, createEffect, createSignal, For, JSXElement, onMount, Show, on } from 'solid-js';
import { createMapStore } from '~/lib/rmap';

interface NodeProps {
	x: number;
	y: number;
	title: string;
	id: string;
	content: JSXElement;
};

interface EdgeProps {
	from: string;
	to: string;
	label?: string;
};

interface DiGraphProps {
	nodes: Array<NodeProps>;
	edges: Array<EdgeProps>;
};

interface NodePropsI {
	pprops: NodeProps;
	onChange: Function;
};

interface EdgePropsI {
	sX: number;
	sY: number;
	eX: number;
	eY: number;
	label?: string;
};

const DiNode : Component<NodePropsI> = (props) => {
	const [dragging, setDragging] = createSignal<boolean>(false);
	const [pos, setPos] = createSignal<{ x: number, y: number}>({ x: props.pprops.x, y: props.pprops.y });
	const [inputHOffset, setInputHOffset] = createSignal<number>(0);
	const [inputWOffset, setInputWOffset] = createSignal<number>(0);
	let lx: number;
	let ly: number;
	let ref!: HTMLDivElement;

	onMount(() => {
		props.onChange(props.pprops.id, props.pprops.x, props.pprops.y, ref.offsetWidth, ref.offsetHeight);
		setInputHOffset(ref.offsetHeight);
		setInputWOffset(ref.offsetWidth);
	});

	createEffect(() => {
		setInputHOffset(ref.offsetHeight);
		setInputWOffset(ref.offsetWidth);
	});

	return (
		<div
			class={`absolute w-40 bg-gray-200 rounded-md border border-gray-400 shadow-md ${dragging() ? "cursor-grabbing" : "cursor-grab"}`} 
			style={{ transform: `translate(${pos().x}px, ${pos().y}px)` }}
			ref={ref}
			onMouseDown={(e) => {
				setDragging(true);
				lx = e.clientX - pos().x;
				ly = e.clientY - pos().y;
			}}
			onMouseMove={(e) => {
				if(dragging()) {
					const nx = e.clientX - lx;
					const ny = e.clientY - ly;
					setPos({ x: nx, y: ny });
					props.onChange(props.pprops.id, nx, ny, ref.offsetWidth, ref.offsetHeight);
				}
			}}
			onMouseUp={() => {
				setDragging(false);
			}}
		>
			<svg class="absolute overflow-visible w-1 h-1">
				<circle cx={0} cy={inputHOffset() / 2} r="5" fill="blue"/>
				<circle cx={inputWOffset()} cy={inputHOffset() / 2} r="5" fill="green"/>
			</svg>
			<div class="font-medium text-center border-b bg-gray-300 border-gray-400 rounded-md">{props.pprops.title}</div>
			<div class="font-small p-2">{props.pprops.content}</div>
		</div>
	);
};

const DiEdge : Component<EdgePropsI> = (props) => {

	const [width, setWidth] = createSignal<number>(0);
	const [height, setHeight] = createSignal<number>(0);

	createEffect(() => {
		setWidth(Math.abs(props.sX - props.eX));
		setHeight(Math.abs(props.sY - props.eY));
	});

	function findQuadrant() : number {
		if(props.sX <= props.eX) {
			return (props.sY <= props.eY) ? 1 : 3;
		}
		return (props.sY <= props.eY) ? 2 : 4;
	}

	function calculatePath() : string {
		switch(findQuadrant()) {
			case 1: {
				return `M 0 0 ${props.eX - props.sX} ${props.eY - props.sY}`;
			}
			case 2: {
				return `M ${props.sX - props.eX} 0 0 ${props.eY - props.sY}`;
			}
			case 3: {
				return `M 0 ${props.sY - props.eY} ${props.eX - props.sX} 0`;
			}
			case 4: {
				return `M ${props.sX - props.eX} ${props.sY - props.eY} 0 0`;
			}
		}
		return '';
	}

	return (
		<div
			class="absolute"
			style={{ transform: `
				translate(${props.sX < props.eX ? props.sX : props.eX}px, ${props.sY < props.eY ? props.sY : props.eY}px)
			`}}>
			<svg width={width() + 10} height={height() + 10} class="overflow-visible">
				<defs>
					<marker 
						id='head' 
						orient="auto" 
						markerWidth='3' 
						markerHeight='4' 
						refX='1' 
						refY='2'
					>
						<path d='M0,0 V4 L2,2 Z' fill="black"/>
					</marker>
				</defs>
				<path
					id='line'
					// marker-end='url(#head)'
					stroke-width='4'
					fill='none' stroke='black'  
					d={calculatePath()}
				/>
				<Show when={props.label !== undefined}>
					<text dy="-5">
						<textPath href="#line" startOffset="50%" text-anchor="middle">
							{props.label}
						</textPath>
					</text>
				</Show>
			</svg>
		</div>
	);
};

export const DiGraph : Component<DiGraphProps> = (props) => {
	const [neMap, neMapActions] = createMapStore<string, Array<{ index: number, type: string }>>(new Map<string, Array<{ index: number, type: string }>>());
	const [edges, setEdges] = createSignal<Array<EdgePropsI>>([]);
	const [nodes, nodesActions] = createMapStore<string, NodeProps>(new Map<string, NodeProps>());

	function calculateNodeInputLocation(id: string) : [number, number] {
		if(!nodes.data.has(id)) {
			console.error('Node id [', id, '] is not valid.');
			return [0, 0];
		}

		const node = nodes.data.get(id)!;

		// TODO: (Cesar) Calculate actual place of input
		return [node.x, node.y];
	}

	function calculateNodeOutputLocation(id: string) : [number, number] {
		if(!nodes.data.has(id)) {
			console.error('Node id [', id, '] is not valid.');
			return [0, 0];
		}

		const node = nodes.data.get(id)!;

		// TODO: (Cesar) Calculate actual place of output
		return [node.x, node.y];
	}

	function setupEdges() {
		// Populate a lookup table
		const lookup = new Map<string, Array<{ index: number, type: string }>>();
		function setLookup(id: string, i: number, type: string) {
			let initial = new Array<{ index: number, type: string }>();
			if(lookup.has(id)) {
				initial = lookup.get(id)!;
			}
			lookup.set(id, [...initial, { index: i, type: type}]);
		}

		// Set the edges and their lookup
		for(let i = 0; i < props.edges.length; i++) {
			const edge = props.edges[i];
			const [ox, oy] = calculateNodeOutputLocation(edge.from);
			const [ix, iy] = calculateNodeInputLocation(edge.to);
			setEdges((p) => [...p, { sX: ox, sY: oy, eX: ix, eY: iy, label: edge.label }]);
			
			setLookup(edge.from, i, 'output');
			setLookup(edge.to  , i, 'input');
			neMapActions.set(lookup);
		}
	}

	function setupNodes() {
		// Set the nodes
		for(const node of props.nodes) {
			nodesActions.add(node.id, node);
		}
	}

	onMount(() => {
		// Set the nodes
		for(const node of props.nodes) {
			nodesActions.add(node.id, node);
		}

		// Populate a lookup table
		const lookup = new Map<string, Array<{ index: number, type: string }>>();
		function setLookup(id: string, i: number, type: string) {
			let initial = new Array<{ index: number, type: string }>();
			if(lookup.has(id)) {
				initial = lookup.get(id)!;
			}
			lookup.set(id, [...initial, { index: i, type: type}]);
		}

		// Set the edges and their lookup
		for(let i = 0; i < props.edges.length; i++) {
			const edge = props.edges[i];
			const [ox, oy] = calculateNodeOutputLocation(edge.from);
			const [ix, iy] = calculateNodeInputLocation(edge.to);
			setEdges((p) => [...p, { sX: ox, sY: oy, eX: ix, eY: iy, label: edge.label }]);
			
			setLookup(edge.from, i, 'output');
			setLookup(edge.to  , i, 'input');
			neMapActions.set(lookup);
		}
	});

	function onNodeChange(id: string, x: number, y: number, width: number, height: number) {
		const edgesId = neMap.data.get(id)!;
		const p = edges();
		const np = Array.from(p);
		for(const edgeId of edgesId) {
			if(edgeId.type === 'input') {
				np[edgeId.index] = { eX: x, eY: y + height / 2, sX: np[edgeId.index].sX, sY: np[edgeId.index].sY, label: np[edgeId.index].label };
			}
			else {
				np[edgeId.index] = { eX: np[edgeId.index].eX, eY: np[edgeId.index].eY, sX: x + width, sY: y + height / 2, label: np[edgeId.index].label };
			}
		}
		setEdges(np);
	}

	createEffect(on(() => props.edges, () => {
		const p = edges();
		const np = Array.from(p);
		for(let i = 0; i < props.edges.length; i++) {
			np[i] = { eX: np[i].eX, eY: np[i].eY, sX: np[i].sX, sY: np[i].sY, label: props.edges[i].label };
		}
		setEdges(np);
	}));

	createEffect(on(() => props.nodes, () => {
	}));

	// Draw nodes and edges
	return (
		<div class="overflow-hidden flex-1 rounded-md shadow-md bg-gray-100">
			<For each={edges()}>{(edge) => {
				return <DiEdge {...edge}/>;
			}}</For>

			<For each={Array.from(nodes.data.values())}>{(node) => {
				return <DiNode pprops={{...node}} onChange={onNodeChange}/>;
			}}</For>
		</div>
	);
};
