import { Component, createEffect, onMount, Show } from "solid-js";
import { MxSpinner } from "~/api";
import { bps_to_string, hsvToRgb } from "~/lib/utils";
import { MxPopup } from "./Popup";

type NodeId = string;
type EdgeId = string;

// NOTE: (César)
// Node ids are like following for identification:
// <type>-<number>
// -> evt-0, evt-1, ...
// -> bck-0, bck-1, ...

interface Node {
	title: string;
	id: NodeId;
};

interface Edge {
	source: NodeId;
	target: NodeId;
	label?: string;
	throughput?: number;
};

export type {
	Node as DiNode,
	Edge as DiEdge
};

interface NodeInternal {
	x: number;
	y: number;
};

interface EdgeInternal {
	extraHeight: number;
};

export const DiGraph: Component<{ nodes: Array<Node>, edges?: Array<Edge> }> = (props) => {
	// Element references
	let canvas!: HTMLCanvasElement;
	let glCanvas!: HTMLCanvasElement;
	let bgCanvas!: HTMLCanvasElement;
	let wrap!: HTMLDivElement;
	let ctx!: CanvasRenderingContext2D;
	let gl!: WebGL2RenderingContext;
	let bgctx!: CanvasRenderingContext2D;

	// Colors
	const CBG_0 = '#f3f4f6';
	const CBG_1 = '#e3e4e6';
	const CTEXT = '#09090b';
	const CLINE_0 = '#b4b4b7';
	const CLINE_1 = '#d4d4d7';
	const CLINE_2 = '#545457';
	const CLINE_3 = '#f4f4f7';

	// Sizes
	const NODE_W = 256;
	const NODE_H = 128;
	const NODE_HEADER_H = 20;
	const EDGE_W = 3;
	const NODE_P = NODE_W / 16;
	const LABEL_W = 128;
	const LABEL_H = 25;
	const BG_XLINES = 20;
	const LABEL_EXPAND_H = 50;
	const LABEL_EXPAND_RATE = 10;
	const LABEL_DISPLAY_THR = 200;

	// Containers
	const nodes = new Map<NodeId, NodeInternal>();
	const edges = new Map<EdgeId, EdgeInternal>();
	let persistentNodePosition = new Map<NodeId, NodeInternal>();
	const animations = new Array<{ callback: Function, frame: number }>;

	// Other
	let lastFrameTime = performance.now();
	const view = { x: 0, y: 0 };
	const mouseLastFrame = { x: 0, y: 0 };
	let gxoff = 0;
	let gyoff = 0;

	// OpenGL stuff
	let glloc = {
		p0: null as WebGLUniformLocation | null,
		p1: null as WebGLUniformLocation | null,
		p2: null as WebGLUniformLocation | null,
		p3: null as WebGLUniformLocation | null,
		time: null as WebGLUniformLocation | null,
		resolution: null as WebGLUniformLocation | null,
		ewidth: null as WebGLUniformLocation | null,
		view: null as WebGLUniformLocation | null
	};
	const SEGMENTS = 64;
	let vertexCount = 0;
	let vao: WebGLVertexArrayObject;
	let program: WebGLProgram;

	const FRAG_SRC = 
	`#version 300 es
	precision highp float;

	in float vT;        // 0 → 1 along the curve
	in float vSide;     // -1 → +1 across thickness

	uniform float u_time;

	out vec4 outColor;

	void main()
	{
		// === FLOW POSITION ===
		float speed = 0.001;
		float repeats = 1.0;

		float flow = fract(vT * repeats - u_time * speed);

		// === SHAPE THE PULSE ===
		float core = smoothstep(0.15, 0.5, flow) -
			smoothstep(0.5, 0.55, flow);

		// === SOFT EDGE ACROSS THICKNESS ===
		float edgeFade = 1.0 - abs(vSide);
		edgeFade = smoothstep(0.0, 0.5, edgeFade);

		// === COLORS ===
		vec3 base = vec3(0.1, 0.1, 0.1);     // dark line
		vec3 glow = vec3(0.0, 1.0, 0.3);     // green flow

		vec3 color = mix(base, glow, core);

		float alpha = 0.9 * edgeFade;

		outColor = vec4(color, alpha);
	}
	`;
	const VERT_SRC = 
	`#version 300 es
	precision highp float;

	layout(location = 0) in float a_t;     // curve position (0 → 1)
	layout(location = 1) in float a_side;  // -1 or +1

	uniform vec2 u_p0;
	uniform vec2 u_p1;
	uniform vec2 u_p2;
	uniform vec2 u_p3;

	uniform float u_thickness;
	uniform vec2 u_resolution;
	uniform vec2 u_view;

	out float vT;
	out float vSide;

	vec2 bezier(float t)
	{
		float u = 1.0 - t;
		return
		u*u*u*u_p0 +
			3.0*u*u*t*u_p1 +
			3.0*u*t*t*u_p2 +
			t*t*t*u_p3;
	}

	vec2 bezierTangent(float t)
	{
		float u = 1.0 - t;
		return
		-3.0*u*u*u_p0 +
			(3.0*u*u - 6.0*u*t)*u_p1 +
			(6.0*u*t - 3.0*t*t)*u_p2 +
			3.0*t*t*u_p3;
	}

	vec2 toClip(vec2 pos)
	{
		vec2 clip = (pos / u_resolution) * 2.0 - 1.0;
		clip.y *= -1.0;
		return clip;
	}

	void main()
	{
		vec2 pos = bezier(a_t);
		vec2 tan = normalize(bezierTangent(a_t));

		// perpendicular
		vec2 normal = vec2(-tan.y, tan.x);

		pos += normal * a_side * u_thickness + u_view;

		// screen → clip space
		vec2 clip = toClip(pos);

		gl_Position = vec4(clip, 0.0, 1.0);

		vT = a_t;
		vSide = a_side;
	}
	`;

	
	// TODO: (César): Build tree instead of using naive getNextAvailablePos
	function buildNodeTree() {
	}

	function restoreNodePos(id: string) {
		return persistentNodePosition.get(id);
	}

	function getNextAvailablePos() {
		const pos = { x: gxoff, y: gyoff };

		pos.x = Math.max(10, pos.x);
		pos.y = Math.max(10, pos.y);

		gxoff += NODE_W * 2.5;
		gyoff += NODE_H * 0.5;
		return pos;
	}

	function getEdgeKey(edge: Edge) {
		return edge.source + '-' + edge.target;
	}

	createEffect(() => {
		for(const node of props.nodes) {
			if(!nodes.has(node.id)) {
				nodes.set(node.id, restoreNodePos(node.id) ?? getNextAvailablePos());
				persistentNodePosition.set(node.id, nodes.get(node.id)!);
			}
		}
	});

	createEffect(() => {
		if(!props.edges) return;
		for(const edge of props.edges) {
			const key = getEdgeKey(edge);
			if(!edges.has(key)) {
				edges.set(key, { extraHeight: 0 });
			}
		}
	});

	onMount(() => {
		ctx = canvas.getContext('2d')!;
		gl = glCanvas.getContext('webgl2', { alpha: true })!;
		bgctx = bgCanvas.getContext('2d')!;

		const rawnp = localStorage.getItem('persistent-node-position');
		if(rawnp) {
			persistentNodePosition = new Map(JSON.parse(rawnp));
		}

		const rawview = localStorage.getItem('persistent-view');
		if(rawview) {
			const lview = JSON.parse(rawview);
			view.x = lview.x;
			view.y = lview.y;
		}

		wrap.classList.add('cursor-move');

		const dpr = window.devicePixelRatio || 1;
		canvas.width = canvas.clientWidth * dpr;
		canvas.height = canvas.clientHeight * dpr;
		ctx.scale(dpr, dpr);

		gl.clearColor(0, 0, 0, 0);
		glCanvas.width = canvas.clientWidth * dpr;
		glCanvas.height = canvas.clientHeight * dpr;

		gl.viewport(0, 0, glCanvas.width, glCanvas.height);

		initGL();

		// Setup canvas callbacks
		let hoveringNode: NodeInternal | undefined = undefined;
		let downNode: NodeInternal | undefined = undefined;
		let mouseDown: { x: number, y: number } | undefined = undefined;
		let atLabel: Edge | undefined = undefined;
		canvas.addEventListener('mousemove', (event: MouseEvent) => {
			const p = mousePos(event);
			const md = getMouseDelta(event);

			if(!atLabel) {
				if(mouseDown && downNode) {
					downNode.x += p.x - mouseDown.x;
					downNode.y += p.y - mouseDown.y;
					mouseDown = p;
				}
				else if(mouseDown) {
					view.x += md.dx;
					view.y += md.dy;
				}
			}

			atLabel = undefined;

			wrap.classList.replace('cursor-grab', 'cursor-move');
			wrap.classList.replace('cursor-pointer', 'cursor-move');
			hoveringNode = undefined;
			for(const node of nodes.values()) {
				if(intersectNode(p, node)) {
					wrap.classList.replace('cursor-move', 'cursor-grab');
					hoveringNode = node;
					return;
				}
			}

			if(props.edges) {
				for(const edge of props.edges) {
					if(intersectEdgeLabel(p, edge)) {
						wrap.classList.replace('cursor-move', 'cursor-pointer');
						atLabel = edge;
						return;
					}
				}
			}
		});

		canvas.addEventListener('mousedown', (event: MouseEvent) => {
			if(event.button !== 0) return;
			downNode = hoveringNode;
			if(downNode) {
				wrap.classList.replace('cursor-grab', 'cursor-grabbing');
			}
			mouseDown = mousePos(event);
		});

		canvas.addEventListener('mouseup', (event: MouseEvent) => {
			if(event.button !== 0) return;

			if(downNode) {
				localStorage.setItem('persistent-node-position', JSON.stringify([...persistentNodePosition]));
			}
			else {
				localStorage.setItem('persistent-view', JSON.stringify(view));
			}

			if(atLabel) {
				const key = getEdgeKey(atLabel);
				const iedge = edges.get(key);
				if(iedge) {
					const operation = iedge.extraHeight >= LABEL_EXPAND_H ?
						() => iedge.extraHeight -= LABEL_EXPAND_RATE :
						() => iedge.extraHeight += LABEL_EXPAND_RATE;
					const predicate = iedge.extraHeight >= LABEL_EXPAND_H ?
						() => iedge.extraHeight > 0 :
						() => iedge.extraHeight <= LABEL_EXPAND_H;
					pushAnimationPredicate(operation, predicate);
				}
			}

			downNode = undefined;
			mouseDown = undefined;
			wrap.classList.replace('cursor-grabbing', 'cursor-grab');
		});

		resize();
	});

	/*
	function pushAnimation(callback: Function) {
		animations.push({ callback: callback, frame: 0 });
	}
	*/

	function pushAnimationPredicate(callback: Function, predicate: Function) {
		animations.push({
			callback: (frame: number, self: number) => {
				callback(frame, self);
				if(!predicate()) popAnimation(self);
			},
			frame: 0
		});
	}

	function popAnimation(id: number) {
		animations.splice(id, 1);
	}

	function mousePos(event: MouseEvent) {
		const rect = canvas.getBoundingClientRect();
		const x = event.clientX - rect.left - view.x;
		const y = event.clientY - rect.top - view.y;
		return { x, y };
	}

	function getMouseDelta(event: MouseEvent) {
		const rect = canvas.getBoundingClientRect();
		const x = event.clientX - rect.left;
		const y = event.clientY - rect.top;

		const dx = x - mouseLastFrame.x;
		const dy = y - mouseLastFrame.y;

		mouseLastFrame.x = x;
		mouseLastFrame.y = y;

		return { dx, dy };
	}

	function intersectNode(mpos: { x: number, y: number }, node: NodeInternal) {
		return mpos.x >= node.x && mpos.x <= node.x + NODE_W && mpos.y >= node.y && mpos.y <= node.y + NODE_H;
	}

	function intersectEdgeLabel(mpos: { x: number, y: number }, edge: Edge) {
		if(!nodes.has(edge.source) || !nodes.has(edge.target)) return false;

		const sourceNode = nodes.get(edge.source)!;
		const targetNode = nodes.get(edge.target)!;

		const sx = sourceNode.x + NODE_W;
		const sy = sourceNode.y + NODE_H / 2;
		const tx = targetNode.x;
		const ty = targetNode.y + NODE_H / 2;

		if(Math.hypot(sx - tx, sy - ty) <= LABEL_DISPLAY_THR) {
			return false;
		}

		const cp = getBezierControlPoints(sx, sy, tx, ty);
		const bangle = getBezierMidpointAngle(sx, sy, tx, ty, cp);

		const cx = (sx + tx) / 2;
		const cy = (sy + ty) / 2;

		const dx = mpos.x - cx;
		const dy = mpos.y - cy;

		const ct = Math.cos(bangle);
		const st = Math.sin(bangle);

		const lx = dx * ct + dy * st;
		const ly = -dx * st + dy * ct

		const key = getEdgeKey(edge);
		const extraH = edges.get(key)?.extraHeight ?? 0;

		return Math.abs(lx) <= LABEL_W / 2 && Math.abs(ly - extraH / 2) <= (LABEL_H / 2) + extraH / 2;
	}

	function drawBG() {
		const ylines = bgCanvas.height / bgCanvas.width * BG_XLINES;
		const size = bgCanvas.width / BG_XLINES;

		for(let i = -2; i < BG_XLINES + 2; i++) {
			const x = ((i + 0.5) / BG_XLINES) * bgCanvas.width;

			bgctx.beginPath();
			bgctx.strokeStyle = CLINE_1;
			bgctx.moveTo(x, -2 * size);
			bgctx.lineTo(x, bgCanvas.height + 2 * size);
			bgctx.stroke();
			bgctx.closePath();
		}
		for(let j = -2; j < ylines + 2; j++) {
			const y = ((j + 0.5) / ylines) * bgCanvas.height;

			bgctx.beginPath();
			bgctx.strokeStyle = CLINE_1;
			bgctx.moveTo(-2 * size, y);
			bgctx.lineTo(bgCanvas.width + 2 * size, y);
			bgctx.stroke();
			bgctx.closePath();
		}
	}

	function getNodeType(node: Node) {
		return node.id.split('-')[0];
	}

	function getNodeColor(node: Node) {
		return getNodeType(node) === 'evt' ?
			hsvToRgb(10, 0.8, 0.7) :
			hsvToRgb(120, 0.8, 0.6);
	}

	function drawNode(node: NodeInternal, enode: Node) {
		const type = getNodeType(enode);
		const color = getNodeColor(enode);

		ctx.fillStyle = color + 'C0';
		ctx.beginPath();
		ctx.roundRect(node.x, node.y, NODE_W, NODE_H, 4);
		ctx.fill();
		ctx.closePath();

		ctx.fillStyle = color + 'F0';
		ctx.strokeStyle = CLINE_0;
		ctx.beginPath();
		ctx.roundRect(node.x, node.y, NODE_W, NODE_HEADER_H, 4);
		ctx.fill();
		ctx.closePath();

		ctx.strokeStyle = CTEXT;
		ctx.beginPath();
		ctx.roundRect(node.x, node.y, NODE_W, NODE_H, 4);
		ctx.stroke();
		ctx.closePath();

		const nodeTitle = type === 'evt' ? 'Event' : 'Client';
		ctx.font = '17px monospace';
		ctx.textBaseline = 'middle';
		ctx.textAlign = 'center';
		ctx.fillStyle = CTEXT;
		ctx.fillText(nodeTitle, node.x + NODE_W / 2, node.y + NODE_HEADER_H / 2);

		const TEXT_OFFSET = 16;
		let offset = 0;
		const nodeId = '  Id: 0x' + enode.id;
		ctx.font = '17px monospace';
		ctx.textBaseline = 'top';
		ctx.textAlign = 'left';
		ctx.fillStyle = CTEXT;
		ctx.fillText(nodeId, node.x + NODE_P, node.y + NODE_HEADER_H + NODE_P / 2 + offset);
		offset += TEXT_OFFSET;

		const nodeName = 'Name: ' + enode.title;
		ctx.font = '17px monospace';
		ctx.textBaseline = 'top';
		ctx.textAlign = 'left';
		ctx.fillStyle = CTEXT;
		ctx.fillText(nodeName, node.x + NODE_P, node.y + NODE_HEADER_H + NODE_P / 2 + offset);
		offset += TEXT_OFFSET;
	}

	function getBezierControlPoints(sx: number, sy: number, tx: number, ty: number) {
		return {
			x1: sx + 100,
			y1: sy,
			x2: tx - 100,
			y2: ty,
		};
	}

	function createShader(gl: WebGL2RenderingContext, type: number, src: string) {
		const s = gl.createShader(type)!;
		gl.shaderSource(s, src);
		gl.compileShader(s);

		if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
			console.error(gl.getShaderInfoLog(s));
		}

		return s;
	}

	function createProgram(vsSrc: string, fsSrc: string) {
		const vs = createShader(gl, gl.VERTEX_SHADER, vsSrc);
		const fs = createShader(gl, gl.FRAGMENT_SHADER, fsSrc);

		const prog = gl.createProgram()!;
		gl.attachShader(prog, vs);
		gl.attachShader(prog, fs);
		gl.linkProgram(prog);

		return prog;
	}

	function initGeometry() {
		const data: number[] = [];

		for (let i = 0; i < SEGMENTS; i++) {
			const t0 = i / SEGMENTS;
			const t1 = (i + 1) / SEGMENTS;

			data.push(
				t0, -1,
				t0,  1,
				t1, -1,

				t1, -1,
				t0,  1,
				t1,  1
			);
		}

		vertexCount = data.length / 2;

		const buffer = gl.createBuffer()!;
		gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(data), gl.STATIC_DRAW);

		vao = gl.createVertexArray()!;
		gl.bindVertexArray(vao);

		gl.enableVertexAttribArray(0);
		gl.vertexAttribPointer(0, 1, gl.FLOAT, false, 8, 0); // t

		gl.enableVertexAttribArray(1);
		gl.vertexAttribPointer(1, 1, gl.FLOAT, false, 8, 4); // side
	}

	function initGL() {
		program = createProgram(VERT_SRC, FRAG_SRC);

		glloc.p0 = gl.getUniformLocation(program, "u_p0");
		glloc.p1 = gl.getUniformLocation(program, "u_p1");
		glloc.p2 = gl.getUniformLocation(program, "u_p2");
		glloc.p3 = gl.getUniformLocation(program, "u_p3");

		glloc.time = gl.getUniformLocation(program, "u_time");
		glloc.resolution = gl.getUniformLocation(program, "u_resolution");
		glloc.ewidth = gl.getUniformLocation(program, "u_thickness");
		glloc.view = gl.getUniformLocation(program, "u_view");

		initGeometry();
	}

	function setupGLRenderThisFrame() {
		gl.clear(gl.COLOR_BUFFER_BIT);
		gl.useProgram(program);
		gl.bindVertexArray(vao);

		gl.uniform2f(glloc.view, view.x, view.y);
		gl.uniform2f(glloc.resolution, glCanvas.width, glCanvas.height);
		gl.uniform1f(glloc.time, performance.now());
		gl.uniform1f(glloc.ewidth, EDGE_W);
	}

	function getBezierMidpointAngle(
		sx: number, sy: number,
		tx: number, ty: number,
		cp: { x1: number, y1: number, x2: number, y2: number }
	) {
		const Ax = (sx + cp.x1) / 2;
		const Bx = (cp.x1 + cp.x2) / 2;
		const Cx = (cp.x2 + tx) / 2;

		const Ay = (sy + cp.y1) / 2;
		const By = (cp.y1 + cp.y2) / 2;
		const Cy = (cp.y2 + ty) / 2;

		const Dx = (Ax + Bx) / 2;
		const Dy = (Ay + By) / 2;

		const Ex = (Bx + Cx) / 2;
		const Ey = (By + Cy) / 2;

		const dx = Ex - Dx;
		const dy = Ey - Dy;

		return Math.atan2(dy, dx);
	}

	function drawFlowAnimatedBezier(sx: number, sy: number, tx: number, ty: number, cp: { x1: number, y1: number, x2: number, y2: number }) {
		gl.uniform2f(glloc.p0, sx, sy);
		gl.uniform2f(glloc.p1, cp.x1, cp.y1);
		gl.uniform2f(glloc.p2, cp.x2, cp.y2);
		gl.uniform2f(glloc.p3, tx, ty);
		gl.drawArrays(gl.TRIANGLES, 0, vertexCount);
	}

	function drawEdge(edge: Edge) {
		const sourceNode = nodes.get(edge.source);
		const targetNode = nodes.get(edge.target);

		const sourceNodeP = props.nodes.find(n => n.id === edge.source);
		const targetNodeP = props.nodes.find(n => n.id === edge.target);

		if(!sourceNode || !targetNode || sourceNode === targetNode || !sourceNodeP || !targetNodeP) return;

		const sx = sourceNode.x + NODE_W;
		const sy = sourceNode.y + NODE_H / 2;
		const tx = targetNode.x;
		const ty = targetNode.y + NODE_H / 2;

		const colorSource = getNodeColor(sourceNodeP);
		ctx.fillStyle = colorSource + 'FF';
		ctx.strokeStyle = CLINE_0;
		ctx.beginPath();
		ctx.arc(sx, sy, 5, -Math.PI / 2, Math.PI / 2);
		ctx.fill();
		ctx.stroke();
		ctx.closePath();

		const colorTarget = getNodeColor(targetNodeP);
		ctx.fillStyle = colorTarget + 'FF';
		ctx.strokeStyle = CLINE_0;
		ctx.beginPath();
		ctx.arc(tx, ty, 5, -Math.PI / 2, Math.PI / 2, true);
		ctx.fill();
		ctx.stroke();
		ctx.closePath();

		const cp = getBezierControlPoints(sx, sy, tx, ty);

		// Render label
		if(Math.hypot(sx - tx, sy - ty) > LABEL_DISPLAY_THR) {
			const key = getEdgeKey(edge);
			const iedge = edges.get(key);
			const extraH = (iedge?.extraHeight ?? 0);
			const bangle = getBezierMidpointAngle(sx, sy, tx, ty, cp);
			const offset = (Math.abs(bangle) > Math.PI / 2) ? Math.PI : 0.0;
			ctx.fillStyle = hsvToRgb(120, 0.7, 0.7) + 'F0';
			const lx = (tx + sx) / 2;
			const ly = (ty + sy) / 2;
			ctx.save();
			ctx.beginPath();
			ctx.translate(lx, ly);
			ctx.rotate(bangle + offset);
			ctx.roundRect(-LABEL_W / 2, -LABEL_H / 2, LABEL_W, LABEL_H + extraH, 3);
			ctx.fill();
			ctx.closePath();
			if(edge.label) {
				ctx.font = '12px monospace';
				ctx.textAlign = 'center';
				ctx.textBaseline = 'middle';
				ctx.fillStyle = CTEXT;
				ctx.fillText(edge.label, 0, 0, LABEL_W - 2);

				if(extraH >= LABEL_EXPAND_H) {
					const pad = 35;
					const size = 25;
					ctx.textAlign = 'center';
					ctx.textBaseline = 'bottom';
					ctx.fillText('Throughtput', 0, pad, LABEL_W - 2);
					ctx.fillStyle = CBG_0;
					ctx.beginPath();
					ctx.roundRect(-LABEL_W / 2 + 10, pad + 2, LABEL_W - 20, size, 3);
					ctx.fill();
					ctx.closePath();
					ctx.textBaseline = 'middle';
					ctx.fillStyle = CTEXT;
					ctx.fillText(edge.throughput && bps_to_string(edge.throughput, false) || 'unknown', 0, pad + 2 + size / 2);
				}
			}
			ctx.restore();
		}

		drawFlowAnimatedBezier(sx, sy, tx, ty, cp);
	}

	function translateView(render: Function) {
		ctx.save();
		bgctx.save();

		const size = bgCanvas.width / BG_XLINES;
		ctx.translate(view.x, view.y);
		bgctx.translate(view.x % size, view.y % size);

		render();

		ctx.restore();
		bgctx.restore();
	}

	function draw(now: number | null) {
		if(now) {
			const dt = now - lastFrameTime;
			if(dt < 1000 / 60.0) {
				requestAnimationFrame(draw);
				return;
			}

			lastFrameTime = now;
		}

		ctx.clearRect(0, 0, canvas.width, canvas.height);
		bgctx.clearRect(0, 0, bgCanvas.width, bgCanvas.height);

		// Update animations (Frame based for now)
		for(const [i, anim] of animations.entries()) {
			anim.callback(anim.frame++, i);
		}

		translateView(() => {
			drawBG();
			
			setupGLRenderThisFrame();
			if(props.edges) for(const edge of props.edges) { drawEdge(edge); }

			for(const node of props.nodes) {
				const inode = nodes.get(node.id);
				inode && drawNode(inode, node);
			}

			requestAnimationFrame(draw);
		});
	}

	function resize() {
		canvas.width = wrap.clientWidth;
		canvas.height = 500;

		bgCanvas.width = wrap.clientWidth;
		bgCanvas.height = 500;

		glCanvas.width = wrap.clientWidth;
		glCanvas.height = 500;
		gl.viewport(0, 0, glCanvas.width, glCanvas.height);
		draw(null);
	}

	return (
		<div>
			<div ref={wrap} class='relative'>
				<canvas ref={bgCanvas} class='rounded-lg absolute inset-0 pointer-events-none bg-gray-100'/>
				<canvas ref={glCanvas} class='rounded-lg absolute inset-0 pointer-events-none'/>
				<canvas ref={canvas} class='rounded-lg shadow-md hover:shadow-lg absolute inset-0'/>
				<Show when={props.nodes.length === 0}>
					<div class='absolute inset-0 flex items-center justify-center h-32'>
						<div class='py-5 bg-gray-200 w-1/4 rounded-lg shadow-md'>
							<MxSpinner description={'Waiting for events...'}/>
						</div>
					</div>
				</Show>
			</div>
		</div>
	);
};
