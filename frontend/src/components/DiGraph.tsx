import { Component, onMount } from "solid-js";
import { hsvToRgb } from "~/lib/utils";

type NodeId = string;

// NOTE: (César)
// Node ids are like following for identification:
// <type>-<number>
// -> evt-0, evt-1, ...
// -> bck-0, bck-1, ...

interface Node {
	x: number;
	y: number;
	title: string;
	id: NodeId;
};

interface Edge {
	source: NodeId;
	target: NodeId;
	label?: string;
};

export const DiGraph: Component<{ nodes: Array<Node>, edges?: Array<Edge> }> = (props) => {
	// Element references
	let canvas!: HTMLCanvasElement;
	let glCanvas!: HTMLCanvasElement;
	let wrap!: HTMLDivElement;
	let ctx!: CanvasRenderingContext2D;
	let gl!: WebGL2RenderingContext;

	// Colors
	const CBG_0 = '#f3f4f6';
	const CBG_1 = '#e3e4e6';
	const CTEXT = '#09090b';
	const CLINE_0 = '#b4b4b7';
	const CLINE_1 = '#d4d4d7';
	const CLINE_2 = '#545457';

	// Sizes
	const NODE_W = 256;
	const NODE_H = 128;
	const NODE_HEADER_H = 20;
	const EDGE_W = 3;

	// Containers
	const nodes = new Array<Node>();
	const edges = new Array<Edge>();

	// Other
	let lastFrameTime = performance.now();

	// OpenGL stuff
	let glloc = {
		p0: null as WebGLUniformLocation | null,
		p1: null as WebGLUniformLocation | null,
		p2: null as WebGLUniformLocation | null,
		p3: null as WebGLUniformLocation | null,
		time: null as WebGLUniformLocation | null,
		resolution: null as WebGLUniformLocation | null,
		ewidth: null as WebGLUniformLocation | null,
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

		pos += normal * a_side * u_thickness;

		// screen → clip space
		vec2 clip = toClip(pos);

		gl_Position = vec4(clip, 0.0, 1.0);

		vT = a_t;
		vSide = a_side;
	}
	`;

	onMount(() => {
		ctx = canvas.getContext('2d')!;
		gl = glCanvas.getContext('webgl2', { alpha: true })!;

		const dpr = window.devicePixelRatio || 1;
		canvas.width = canvas.clientWidth * dpr;
		canvas.height = canvas.clientHeight * dpr;
		ctx.scale(dpr, dpr);

		gl.clearColor(0, 0, 0, 0);
		glCanvas.width = canvas.clientWidth * dpr;
		glCanvas.height = canvas.clientHeight * dpr;

		gl.viewport(0, 0, glCanvas.width, glCanvas.height);

		initGL();

		nodes.push({
			x: 100,
			y: 100,
			title: 'My Node',
			id: 'evt-0'
		});

		nodes.push({
			x: 700,
			y: 300,
			title: 'My Bck',
			id: 'bck-0'
		});

		nodes.push({
			x: 900,
			y: 50,
			title: 'My Bck',
			id: 'bck-1'
		});

		edges.push({
			source: 'evt-0',
			target: 'bck-0'
		});

		edges.push({
			source: 'bck-0',
			target: 'bck-1'
		});


		// Setup canvas callbacks
		let hoveringNode: Node | undefined = undefined;
		let downNode: Node | undefined = undefined;
		let mouseDown: { x: number, y: number } | undefined = undefined;
		canvas.addEventListener('mousemove', (event: MouseEvent) => {
			const p = mousePos(event);

			if(mouseDown && downNode) {
				downNode.x += p.x - mouseDown.x;
				downNode.y += p.y - mouseDown.y;
				mouseDown = p;
			}

			wrap.classList.replace('cursor-grab', 'cursor-default');
			hoveringNode = undefined;
			for(const node of nodes) {
				if(intersectNode(p, node)) {
					wrap.classList.add('cursor-grab');
					hoveringNode = node;
					break;
				}
			}
		});

		canvas.addEventListener('mousedown', (event: MouseEvent) => {
			if(event.button !== 0) return;
			downNode = hoveringNode;
			mouseDown = mousePos(event);
		});

		canvas.addEventListener('mouseup', (event: MouseEvent) => {
			if(event.button !== 0) return;
			downNode = undefined;
		});

		resize();
	});

	function mousePos(event: MouseEvent) {
		const rect = canvas.getBoundingClientRect();
		const x = event.clientX - rect.left;
		const y = event.clientY - rect.top;
		return { x, y };
	}

	function intersectNode(mpos: { x: number, y: number }, node: Node) {
		return mpos.x >= node.x && mpos.x <= node.x + NODE_W && mpos.y >= node.y && mpos.y <= node.y + NODE_H;
	}

	function drawBG() {
		ctx.fillStyle = CBG_0;
		ctx.fillRect(0, 0, canvas.width, canvas.height);
	}

	function getNodeColor(node: Node) {
		const nodeType = node.id.split('-')[0];
		return nodeType === 'evt' ?
			hsvToRgb(10, 0.8, 0.7) :
			hsvToRgb(120, 0.8, 0.6);
	}

	function drawNode(node: Node) {
		const color = getNodeColor(node);

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

		const nodeType = node.id.split('-')[0];
		const nodeTitle = nodeType === 'evt' ? 'Event' : 'Client';
		ctx.font = '17px monospace';
		ctx.textBaseline = 'middle';
		ctx.textAlign = 'center';
		ctx.fillStyle = CTEXT;
		ctx.fillText(nodeTitle, node.x + NODE_W / 2, node.y + NODE_HEADER_H / 2);
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

		initGeometry();
	}

	function setupGLRenderThisFrame() {
		gl.clear(gl.COLOR_BUFFER_BIT);
		gl.useProgram(program);
		gl.bindVertexArray(vao);

		gl.uniform2f(glloc.resolution, glCanvas.width, glCanvas.height);
		gl.uniform1f(glloc.time, performance.now());
		gl.uniform1f(glloc.ewidth, EDGE_W);
	}

	function drawFlowAnimatedBezier(sx: number, sy: number, tx: number, ty: number) {
		const cp = getBezierControlPoints(sx, sy, tx, ty);
		console.log((sx / glCanvas.width) * 2.0 - 1.0, (sy / glCanvas.height) * 2.0 - 1.0);
		gl.uniform2f(glloc.p0, sx, sy);
		gl.uniform2f(glloc.p1, cp.x1, cp.y1);
		gl.uniform2f(glloc.p2, cp.x2, cp.y2);
		gl.uniform2f(glloc.p3, tx, ty);
		gl.drawArrays(gl.TRIANGLES, 0, vertexCount);
	}

	function drawEdge(edge: Edge) {
		const sourceNode = nodes.find(n => n.id === edge.source);
		const targetNode = nodes.find(n => n.id === edge.target);

		if(!sourceNode || !targetNode || sourceNode === targetNode) return;

		const sx = sourceNode.x + NODE_W;
		const sy = sourceNode.y + NODE_H / 2;
		const tx = targetNode.x;
		const ty = targetNode.y + NODE_H / 2;

		// TODO: (César) De-duplicate
		const colorSource = getNodeColor(sourceNode);
		ctx.fillStyle = colorSource + 'FF';
		ctx.strokeStyle = CLINE_0;
		ctx.beginPath();
		ctx.arc(sx, sy, 5, -Math.PI / 2, Math.PI / 2);
		ctx.fill();
		ctx.stroke();
		ctx.closePath();

		// TODO: (César) De-duplicate
		const colorTarget = getNodeColor(targetNode);
		ctx.fillStyle = colorTarget + 'FF';
		ctx.strokeStyle = CLINE_0;
		ctx.beginPath();
		ctx.arc(tx, ty, 5, -Math.PI / 2, Math.PI / 2, true);
		ctx.fill();
		ctx.stroke();
		ctx.closePath();

		drawFlowAnimatedBezier(sx, sy, tx, ty);
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

		drawBG();
		
		setupGLRenderThisFrame();
		for(const edge of edges) { drawEdge(edge); }

		for(const node of nodes) { drawNode(node); }

		requestAnimationFrame(draw);
	}

	function resize() {
		canvas.width = wrap.clientWidth;
		canvas.height = 500;

		glCanvas.width = wrap.clientWidth;
		glCanvas.height = 500;
		gl.viewport(0, 0, glCanvas.width, glCanvas.height);
		draw(null);
	}

	return (
		<div>
			<div ref={wrap} class='relative'>
				<canvas ref={canvas} class='rounded-lg shadow-md hover:shadow-lg absolute inset-0'/>
				<canvas ref={glCanvas} class='rounded-lg absolute inset-0 pointer-events-none'/>
			</div>
		</div>
	);
};
