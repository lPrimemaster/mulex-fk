import { Component, onMount } from "solid-js";
import { hsvToRgb } from "~/lib/utils";

interface Timestamp {
	begin: BigInt;
	end: BigInt;
};

interface TraceRecord {
	group: string;
	name: string;
	timestamp: Timestamp;
	traceid: BigInt;
	clientid: BigInt;
};

interface TraceLane {
	name: string;
	renderIndex?: number;
};

interface Point {
	x: number;
	y: number;
};

interface RenderContext {
	rc: CanvasRenderingContext2D;
	laneCount: number;

	viewStart: number;
	viewEnd: number;
	viewOffset: BigInt;

	titlesz: number;

	renderMode: 'prop' | 'nonprop';
};

const TraceTimeline: Component = () => {
	// Element references
	let canvas!: HTMLCanvasElement;
	let wrap!: HTMLDivElement;

	// Padding and sizes
	const LANE_HEIGHT = 64;
	const LANE_PADDING = 5;
	const TTL_HEADER_HEIGHT = 20;
	const LANE_ML = 10;
	const RECORD_MIN_W = 80;

	// Containers
	const lanes = new Map<BigInt, TraceLane>();
	const records = new Array<TraceRecord>();
	const groupColors = new Map<string, string>([
		['Rpc', hsvToRgb( 10, 1, 0.8) + '90'],
		['Rdb', hsvToRgb(120, 1, 0.8) + '90'],
		['Evt', hsvToRgb(220, 1, 0.8) + '90'],
	]);

	// Colors
	const CBG_0 = '#f3f4f6';
	const CBG_1 = '#e3e4e6';
	const CTEXT = '#09090b';
	const CLINE_0 = '#b4b4b7';
	const CLINE_1 = '#d4d4d7';

	// Callback state
	let mouseDragging = false;

	// Adapted from https://stackoverflow.com/a/13532993
	function shadeColor(color: string, percent: number) {
		let R = parseInt(color.substring(1, 3), 16);
		let G = parseInt(color.substring(3, 5), 16);
		let B = parseInt(color.substring(5, 7), 16);

		R = Math.floor(R * (100 + percent) / 100);
		G = Math.floor(G * (100 + percent) / 100);
		B = Math.floor(B * (100 + percent) / 100);

		R = (R < 255) ? R : 255;  
		G = (G < 255) ? G : 255;  
		B = (B < 255) ? B : 255;  

		R = Math.round(R)
		G = Math.round(G)
		B = Math.round(B)

		let RR = ((R.toString(16).length == 1) ? '0' + R.toString(16) : R.toString(16));
		let GG = ((G.toString(16).length == 1) ? '0' + G.toString(16) : G.toString(16));
		let BB = ((B.toString(16).length == 1) ? '0' + B.toString(16) : B.toString(16));

		return '#' + RR + GG + BB;
	}
	const muted = (x: string) => shadeColor(x, -5);

	function height() {
		return TTL_HEADER_HEIGHT + LANE_HEIGHT * lanes.size;
	}

	function getLaneY(lanenumber: number) {
		return lanenumber * LANE_HEIGHT + TTL_HEADER_HEIGHT;
	}

	function drawLane(ctx: RenderContext, lane: TraceLane) {
		const laney = getLaneY(ctx.laneCount);
		lane.renderIndex = ctx.laneCount;

		// BG on name
		ctx.rc.fillStyle = (ctx.laneCount & 1) ? CBG_0 : CBG_1;
		ctx.rc.fillRect(0, laney, 2 * LANE_ML + ctx.titlesz, LANE_HEIGHT);

		// BG on lane itself
		ctx.rc.fillStyle = (ctx.laneCount & 1) ? muted(CBG_0) : muted(CBG_1);
		ctx.rc.fillRect(2 * LANE_ML + ctx.titlesz, laney, canvas.width, LANE_HEIGHT);

		// Lane name
		ctx.rc.fillStyle = CTEXT;
		ctx.rc.textBaseline = 'middle';
		ctx.rc.textAlign = 'left';
		ctx.rc.font = '14px monospace';
		ctx.rc.fillText(lane.name, LANE_ML, laney + LANE_HEIGHT / 2);

		// Lane vertical boundary 
		ctx.rc.strokeStyle = CLINE_0;
		ctx.rc.lineWidth = 1.5;
		ctx.rc.beginPath();
		ctx.rc.moveTo(2 * LANE_ML + ctx.titlesz, laney);
		ctx.rc.lineTo(2 * LANE_ML + ctx.titlesz, laney + LANE_HEIGHT);
		ctx.rc.stroke();
		ctx.rc.closePath();
		ctx.rc.lineWidth = 1;

		// Lane horizontal boundary
		ctx.rc.beginPath();
		ctx.rc.moveTo(0, laney + LANE_HEIGHT);
		ctx.rc.lineTo(canvas.width, laney + LANE_HEIGHT);
		ctx.rc.stroke();
		ctx.rc.closePath();
	}

	function msToX(ctx: RenderContext, ms: number) {
		const range = ctx.viewEnd - ctx.viewStart;
		const xoff = ctx.titlesz + 2 * LANE_ML;
		return xoff + ((ms - ctx.viewStart) / range) * (canvas.width - xoff);
	}

	function computeLaneStartOffset(ctx: CanvasRenderingContext2D) {
		return Math.max(...lanes.values().toArray().map((l: TraceLane) => ctx.measureText(l.name).width));
	}

	function drawHeader(ctx: RenderContext) {
		ctx.rc.fillStyle = CLINE_1;
		ctx.rc.fillRect(0, 0, canvas.width, TTL_HEADER_HEIGHT);

		// Draw ticks
		//const tickSteps = [0.1, 0.25, 0.5, 1, 2, 5, 10, 25, 50];
		// const step = tickSteps.find(s => );
		const step = 500;
		const firstTick = Math.ceil(0 / step) * step;
		ctx.rc.font = '10px monospace';
		ctx.rc.textAlign = 'center';
		for(let t = firstTick; t <= ctx.viewEnd; t = +(t+step).toFixed(6)) {
			// Compute x
			const x = msToX(ctx, t);
			if(x < ctx.titlesz) continue;

			// time border
			ctx.rc.strokeStyle = muted(CLINE_1);
			ctx.rc.lineWidth = 0.5;
			ctx.rc.beginPath(); ctx.rc.moveTo(x, TTL_HEADER_HEIGHT); ctx.rc.lineTo(x, canvas.height); ctx.rc.stroke();

			// tick draw
			ctx.rc.strokeStyle = muted(CTEXT);
			ctx.rc.beginPath(); ctx.rc.moveTo(x, 16); ctx.rc.lineTo(x, TTL_HEADER_HEIGHT); ctx.rc.stroke();

			// time text
			ctx.rc.fillStyle = muted(CTEXT);
			ctx.rc.fillText(t.toFixed(step < 1 ? 2 : 0)+'ms', x, 10);
		}

		// Draw start line
		ctx.rc.strokeStyle = CTEXT;
		ctx.rc.lineWidth = 1;
		ctx.rc.beginPath();
		ctx.rc.moveTo(0, TTL_HEADER_HEIGHT);
		ctx.rc.lineTo(canvas.width, TTL_HEADER_HEIGHT);
		ctx.rc.stroke();
		ctx.rc.closePath();

		ctx.rc.font = '12px monospace';
		ctx.rc.fillStyle = CTEXT;
		ctx.rc.textAlign = 'center';
		ctx.rc.textBaseline = 'middle';
		ctx.rc.fillText('Provider', ctx.titlesz / 2 + LANE_ML, TTL_HEADER_HEIGHT / 2);
	}

	function isRecordHidden(ctx: RenderContext, record: TraceRecord) {
		return record.timestamp.end.valueOf() < (BigInt(ctx.viewStart) + ctx.viewOffset.valueOf()) ||
			   record.timestamp.begin.valueOf() > (BigInt(ctx.viewEnd) + ctx.viewOffset.valueOf());
	}

	function getRecordRelativeTimestamp(ctx: RenderContext, record: TraceRecord) {
		return {
			begin: Number(record.timestamp.begin.valueOf() - ctx.viewOffset.valueOf()),
			end: Number(record.timestamp.end.valueOf() - ctx.viewOffset.valueOf())
		};
	}

	function getRecordLane(record: TraceRecord) {
		if(!lanes.has(record.clientid)) {
			console.error('Failed to fetch lane from record: ', record);
		}
		return lanes.get(record.clientid);
	}

	function getGroupRecordColor(group: string) {
		if(groupColors.has(group)) {
			return groupColors.get(group)!;
		}

		// TODO: Insert random-ish color for new group
		// 		 (color wheel based ?)
		return undefined;
	}

	function textEllideIfNeeded(ctx: RenderContext, text: string, width: number) {
		if(ctx.rc.measureText(text).width <= width) {
			return text;
		}

		const ellipsis = '…';
		let left = 0;
		let right = text.length;
		while (left < right) {
			const mid = Math.floor((left + right) / 2);
			const substr = text.slice(0, mid) + ellipsis;
			if (ctx.rc.measureText(substr).width <= width) {
				left = mid + 1;
			} else {
				right = mid;
			}
		}

		return text.slice(0, left - 1) + ellipsis;
	}

	function formatTime(ns: number) {
		const units = [
			{ label: "ns", scale: 1 },
			{ label: "us", scale: 1e3 },
			{ label: "ms", scale: 1e6 },
			{ label: "s",  scale: 1e9 },
		];

		let value = ns;
		let unit = "ns";

		for (let i = 0; i < units.length; i++) {
			if (ns < units[i].scale * 1000 || i === units.length - 1) {
				value = ns / units[i].scale;
				unit = units[i].label;
				break;
			}
		}

		return `${value.toFixed(1)}${unit}`;
	}

	function calculateTimeDelta(record: TraceRecord) {
		// in ns
		const ns = Number(record.timestamp.end.valueOf() - record.timestamp.begin.valueOf()) * 1_000_000;

		return formatTime(ns);
	}

	function drawRecord(ctx: RenderContext, record: TraceRecord) {
		// If the record is out of the view skip rendering it
		if(isRecordHidden(ctx, record)) return;

		const rts = getRecordRelativeTimestamp(ctx, record);
		const rxs = Math.max(msToX(ctx, rts.begin), ctx.titlesz + 2 * LANE_ML);
		const rxe = Math.min(msToX(ctx, rts.end), canvas.width);
		let   rw = rxe - rxs;

		const lane = getRecordLane(record);
		if(lane === undefined || lane.renderIndex === undefined) {
			// Found no lane or lane without a valid render index
			// Skip rendering to avoid crashes
			return;
		}

		const ry = getLaneY(lane.renderIndex);

		// Depending on the mode we either draw all records
		// if we are in non-proportional mode or only draw if
		// record ts span > RECORD_MIN_W
		if(ctx.renderMode === 'nonprop' && rw < RECORD_MIN_W) {
			rw = RECORD_MIN_W;
		}
		else if(rw < RECORD_MIN_W) {
			return;
		}

		const color = getGroupRecordColor(record.group)!;

		// Draw shape
		ctx.rc.beginPath();
		ctx.rc.roundRect(rxs, ry + LANE_PADDING, rw, LANE_HEIGHT - LANE_PADDING * 2, 3);
		ctx.rc.stroke();
		ctx.rc.fillStyle = color;
		ctx.rc.fill();
		ctx.rc.closePath();

		// Draw header shape
		ctx.rc.beginPath();
		ctx.rc.roundRect(rxs, ry + LANE_PADDING, rw, LANE_PADDING * 3.5, 3);
		ctx.rc.fillStyle = color;
		ctx.rc.fill();
		ctx.rc.closePath();

		// Draw group
		ctx.rc.textBaseline = 'middle';
		ctx.rc.textAlign = 'center';
		ctx.rc.fillStyle = shadeColor(color, -50);
		ctx.rc.fillText(record.group, rxs + rw / 2, ry + LANE_PADDING * 3);

		if(rw >= 50) {
			// Draw name
			ctx.rc.textBaseline = 'middle';
			ctx.rc.textAlign = 'left';
			ctx.rc.fillStyle = shadeColor(color, -90);
			ctx.rc.fillText(
				textEllideIfNeeded(ctx, '\u{1F3F7}\u{FE0F}' + record.name, rw - LANE_ML),
				rxs + LANE_ML / 2,
				ry + LANE_PADDING * 3 + (LANE_HEIGHT - LANE_PADDING * 3) / 3
			);

			// Draw span
			ctx.rc.textBaseline = 'middle';
			ctx.rc.textAlign = 'left';
			ctx.rc.fillStyle = shadeColor(color, -90);
			ctx.rc.fillText(
				textEllideIfNeeded(
					ctx,
					'\u{23F1}\u{FE0F}' + Number(record.timestamp.begin).toFixed(0) + 'ms' + '\u{26A1}\u{FE0F}' + calculateTimeDelta(record),
					rw - LANE_ML
				),
				rxs + LANE_ML / 2,
				ry + LANE_PADDING * 3 + 2 * (LANE_HEIGHT - LANE_PADDING * 3) / 3
			);
		}
	}

	function draw() {
		// Compute lane name max size
		const lctx = canvas.getContext('2d')!;
		lctx.font = '14px monospace';
		const ctx: RenderContext = {
			rc: lctx,
			laneCount: 0,

			viewStart: 0,
			viewEnd: 10_000,
			viewOffset: BigInt(0),

			titlesz: computeLaneStartOffset(lctx),

			renderMode: 'nonprop'
		};

		const W = canvas.width;
		const H = canvas.height;

		// Background
		ctx.rc.fillStyle = '#e4e4e7';
		ctx.rc.fillRect(0, 0, W, H);

		// Lanes
		for(const [_, lane] of lanes) {
			drawLane(ctx, lane);
			ctx.laneCount++;
		}

		// Header
		drawHeader(ctx);

		// Draw data
		for(const record of records) {
			drawRecord(ctx, record);
		}
	}

	function resize() {
		canvas.width = wrap.clientWidth;
		canvas.height = height();
		draw();
	}

	function mousePos(event: MouseEvent) {
		const rect = canvas.getBoundingClientRect();
		const x = event.clientX - rect.left;
		const y = event.clientY - rect.top;
		return { x, y };
	}

	function addRecord(record: TraceRecord) {
		records.push(record);

		if(!lanes.has(record.clientid)) {
			lanes.set(record.clientid, {
				name: 'Backend' // TODO: (César) fetch name from backend name
			});
		}
	}

	onMount(() => {
		// Setup lane 0 'System'
		lanes.set(0n, { name: 'System' });

		addRecord({
			group: 'Rdb',
			name: 'Read',
			timestamp: { begin: BigInt(1000), end: BigInt(1100) },
			traceid: BigInt(0),
			clientid: BigInt(1)
		});

		addRecord({
			group: 'Rpc',
			name: 'ReadValueDirectVeryBigName',
			timestamp: { begin: BigInt(2000), end: BigInt(3000) },
			traceid: BigInt(0),
			clientid: BigInt(0)
		});

		addRecord({
			group: 'Evt',
			name: 'test_evt',
			timestamp: { begin: BigInt(1000), end: BigInt(2000) },
			traceid: BigInt(0),
			clientid: BigInt(2)
		});

		// Setup canvas callbacks
		canvas.addEventListener('mousemove', (event: MouseEvent) => {
			const p = mousePos(event);

			if(p.y < TTL_HEADER_HEIGHT) {
				wrap.classList.add('cursor-grab');
			}
			else {
				wrap.classList.remove('cursor-grab');
			}
		});

		canvas.addEventListener('mousedown', (event: MouseEvent) => {
			if(event.button !== 0) return;
			const p = mousePos(event);

			if(p.y < TTL_HEADER_HEIGHT) {
				mouseDragging = true;
				wrap.classList.add('cursor-grabbing');
			}
		});

		canvas.addEventListener('mouseup', (event: MouseEvent) => {
			if(event.button !== 0) return;
			mouseDragging = false;
			wrap.classList.remove('cursor-grabbing');
		});

		canvas.addEventListener('mouseleave', (event: MouseEvent) => {
			if(event.button !== 0) return;
			wrap.classList.remove('cursor-grab');
		});

		// Re-draw on div resize
		new ResizeObserver(resize).observe(wrap);
	});

	return (
		<div ref={wrap}>
			<canvas ref={canvas} class='rounded-lg shadow-md hover:shadow-lg'/>
		</div>
	);
};

export default TraceTimeline;
