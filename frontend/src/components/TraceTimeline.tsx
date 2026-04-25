import { Component, createSignal, onMount } from "solid-js";
import { MxDoubleSwitch } from "~/api/Switch";
import { MxGenericType } from "~/lib/convert";
import { MxRdb } from "~/lib/rdb";
import { hsvToRgb } from "~/lib/utils";
import { MxWebsocket } from "~/lib/websocket";
import Card from "./Card";

interface Timestamp {
	begin: number;
	end: number;
};

export interface TraceRecord {
	group: string;
	name: string;
	timestamp: Timestamp;
	traceid: BigInt;
	clientid: BigInt;
	complete: boolean;
	layer: number;
	hidden?: boolean;
};

interface TraceRecordBounds {
	l: number;
	r: number;
	t: number;
	b: number;
	recordReference: TraceRecord;
	laneReference: TraceLane;
};

type TraceLayer = Array<TraceRecord>;

interface TraceLane {
	name: string;
	renderIndex?: number;
	currentLayer: number;
	layers: Array<TraceLayer>;
	hidden: boolean;
};

interface RenderContext {
	rc: CanvasRenderingContext2D;
	laneCount: number;

	viewStart: number;
	viewEnd: number;
	viewOffset: number;

	titlesz: number;

	renderMode: 'prop' | 'nonprop';
};

export const TraceTimeline: Component<{ onRecordSelect?: Function }> = (props) => {
	// Element references
	let canvas!: HTMLCanvasElement;
	let wrap!: HTMLDivElement;

	// Context
	let ctx: RenderContext;

	// Padding and sizes
	const LANE_HEIGHT = 64;
	const LANE_PADDING = 5;
	const TTL_HEADER_HEIGHT = 20;
	const LANE_ML = 10;
	const RECORD_MIN_W_MS = 750;

	// Other constants
	const RECORD_LIFETIME_MS = 120_000;
	const RECORD_MAX_SIZE = 10_000;

	// Containers
	const lanes = new Map<BigInt, TraceLane>();
	const records = new Array<TraceRecord>();
	const recordBounds = new Array<TraceRecordBounds>();
	const incompleteRecords = new Map<BigInt, TraceRecord>();
	const groupColors = new Map<string, string>([
		['Rpc'  , hsvToRgb( 10, 1, 0.8) + '90'],
		['Rdb'  , hsvToRgb(120, 1, 0.8) + '90'],
		['Evt'  , hsvToRgb(220, 1, 0.8) + '90'],
		['Error', hsvToRgb(  5, 1, 0.8) + '90'],
	]);
	let internedStrings: Map<number, string> | undefined = undefined;
	const addRecordWaiting = new Map<BigInt, Promise<boolean>>();
	const systemAliasCids = new Map<BigInt, BigInt>();

	// Colors
	const CBG_0 = '#f3f4f6';
	const CBG_1 = '#e3e4e6';
	const CTEXT = '#09090b';
	const CLINE_0 = '#b4b4b7';
	const CLINE_1 = '#d4d4d7';

	// Callback state
	let mouseDragging = false;
	let mouseX = 0;

	// Shared state between draw state and solidjs
	const [shouldStop, setShouldStop] = createSignal<boolean>(false);
	const [showSysRecords, setShowSysRecords] = createSignal<boolean>(false);

	// Zero time offset
	let timeOffset: BigInt | undefined = undefined;
	let perfStartTime: number = 0;

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

	const laneVisibleCountThisFrame = new Map<string, number>();
	function getLaneVisibleLayersCount(lane: TraceLane) {
		if(laneVisibleCountThisFrame.has(lane.name)) {
			return laneVisibleCountThisFrame.get(lane.name)!;
		}

		let size = 0;
		//for(const layer of lane.layers) {
		for(let i = 0; i < lane.layers.length; i++) {
			const layer = lane.layers[i];
			/*
			// Layer is empty -> hide
			if(layer.length === 0) continue;
			if(i < 3) console.log(i, 'A ');

			// Layer is all to the left of the current view -> hide
			let ts = getRecordRelativeTimestamp(ctx, layer[0]);
			if(ts.begin > ctx.viewEnd) continue;
			if(i < 3) console.log(i, 'B ');

			// Layer is all to the left of the current view -> hide
			ts = getRecordRelativeTimestamp(ctx, layer[layer.length - 1]);
			if(ts.end < ctx.viewStart) continue;
			if(i < 3) console.log(i, 'C ');
			*/

			// Otherwise check one-by-one -> hide
			if(layer.every(x => x.hidden) || lane.hidden) continue;
			size++;
		}

		laneVisibleCountThisFrame.set(lane.name, size);
		return size;
	}

	function height() {
		return TTL_HEADER_HEIGHT + lanes.values()
			.reduce((r: number, x: TraceLane) => r + getLaneVisibleLayersCount(x) * LANE_HEIGHT, 0);
	}

	function getLaneY(lanenumber: number) {
		return TTL_HEADER_HEIGHT + lanes.values()
			.filter(x => (x.renderIndex ?? 0xFFFFFFFF) < lanenumber)
			.reduce((r: number, x: TraceLane) => r + getLaneVisibleLayersCount(x) * LANE_HEIGHT, 0);
	}

	function drawLane(ctx: RenderContext, lane: TraceLane) {
		if(lane.hidden) return;

		const laney = getLaneY(ctx.laneCount);
		lane.renderIndex = ctx.laneCount;

		const laneh = LANE_HEIGHT * getLaneVisibleLayersCount(lane);

		// BG on name
		ctx.rc.fillStyle = (ctx.laneCount & 1) ? CBG_0 : CBG_1;
		ctx.rc.fillRect(0, laney, 2 * LANE_ML + ctx.titlesz, laneh);

		// BG on lane itself
		ctx.rc.fillStyle = (ctx.laneCount & 1) ? muted(CBG_0) : muted(CBG_1);
		ctx.rc.fillRect(2 * LANE_ML + ctx.titlesz, laney, canvas.width, laneh);

		// Lane name
		ctx.rc.fillStyle = CTEXT;
		ctx.rc.textBaseline = 'middle';
		ctx.rc.textAlign = 'left';
		ctx.rc.font = '14px monospace';
		ctx.rc.fillText(lane.name, LANE_ML, laney + laneh / 2);

		// Lane vertical boundary 
		ctx.rc.strokeStyle = CLINE_0;
		ctx.rc.lineWidth = 1.5;
		ctx.rc.beginPath();
		ctx.rc.moveTo(2 * LANE_ML + ctx.titlesz, laney);
		ctx.rc.lineTo(2 * LANE_ML + ctx.titlesz, laney + laneh);
		ctx.rc.stroke();
		ctx.rc.closePath();
		ctx.rc.lineWidth = 1;

		// Lane horizontal boundary
		ctx.rc.beginPath();
		ctx.rc.moveTo(0, laney + laneh);
		ctx.rc.lineTo(canvas.width, laney + laneh);
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
		const step = 500;
		const firstTick = Math.ceil(ctx.viewStart / step) * step;
		ctx.rc.font = '10px monospace';
		ctx.rc.textAlign = 'center';
		for(let t = firstTick; t <= ctx.viewEnd; t = +(t + step).toFixed(6)) {
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
			ctx.rc.fillText(t.toFixed(step < 1 ? 2 : 0) + 'ms', x, 10);
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

	function drawNowLine() {
		// Draw now line
		const nowx = msToX(ctx, serverSyncedTime());
		if(nowx > ctx.titlesz) {
			ctx.rc.lineWidth = 2;
			ctx.rc.beginPath();
			ctx.rc.moveTo(nowx, TTL_HEADER_HEIGHT);
			ctx.rc.lineTo(nowx, canvas.height);
			ctx.rc.stroke();
			ctx.rc.closePath();
		}
	}

	// TODO: (César) This is a bit finnicky
	function serverSyncedTime() {
		return performance.now() - perfStartTime;
	}

	function isRecordHidden(ctx: RenderContext, begin: number, end: number) {
		return end < (ctx.viewStart + Number(ctx.viewOffset.valueOf())) ||
			   begin > (ctx.viewEnd + Number(ctx.viewOffset.valueOf()));
	}

	function getRecordRelativeTimestamp(ctx: RenderContext, record: TraceRecord) {
		return {
			begin: record.timestamp.begin - ctx.viewOffset,
			end: record.timestamp.end - ctx.viewOffset
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

	function splitGraphemes(text: string): string[] {
		if (typeof Intl !== "undefined" && (Intl as any).Segmenter) {
			const seg = new Intl.Segmenter(undefined, { granularity: "grapheme" });
			return Array.from(seg.segment(text), s => s.segment);
		}
		return Array.from(text);
	}

	function textEllideIfNeeded(ctx: RenderContext, text: string, width: number) {
		if(ctx.rc.measureText(text).width <= width) {
			return text;
		}

		const ellipsis = '…';
		const chars = splitGraphemes(text);
		let left = 0;
		let right = chars.length;

		while (left < right) {
			const mid = Math.floor((left + right) / 2);
			const substr = chars.slice(0, mid).join('') + ellipsis;

			if (ctx.rc.measureText(substr).width <= width) {
				left = mid + 1;
			}
			else {
				right = mid;
			}
		}

		return left < 2 ? '' : chars.slice(0, left - 1).join('') + ellipsis;
	}

	function formatTime(ns: number, digits: number = 0) {
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

		return `${value.toFixed(digits)}${unit}`;
	}

	function calculateTimeDelta(record: TraceRecord) {
		// in ns
		const ns = (record.timestamp.end - record.timestamp.begin) * 1e6;
		return formatTime(ns);
	}

	// TODO: (César) Optimize
	function isLayerDrawable(layer: TraceLayer) {
		/*
		for(let i = 0; i < layer.length; i++) {
			console.log('Checking layer: ', i, ' record: ', layer[i].recordReference.complete);
		}
		*/
		return layer.length === 0 || layer.reverse().every(x => x.complete);
	}

	function getLaneFirstAvailableLayer(lane: TraceLane) {
		for(let i = 0; i < lane.layers.length; i++) {
			if(isLayerDrawable(lane.layers[i])) {
				return i;
			}
		}

		lane.layers.push([]);
		return lane.layers.length - 1;
	}

	function drawRecord(ctx: RenderContext, record: TraceRecord) {
		// Depending on the mode we either draw all records
		// if we are in non-proportional mode or only draw if
		// record ts span > RECORD_MIN_W
		const rts = getRecordRelativeTimestamp(ctx, record);
		const diff = rts.end - rts.begin;
		const extraw = (ctx.renderMode === 'nonprop' && diff < RECORD_MIN_W_MS) ? RECORD_MIN_W_MS - diff : 0;
		const rxs = Math.max(msToX(ctx, rts.begin), ctx.titlesz + 2 * LANE_ML);
		const rxe = Math.min(msToX(ctx, rts.end + extraw), canvas.width);
		const rw = rxe - rxs;

		// If the record is out of the view skip rendering it
		record.hidden = isRecordHidden(ctx, rts.begin, rts.end + extraw);
		if(record.hidden) return;

		const lane = getRecordLane(record);
		if(lane === undefined || lane.renderIndex === undefined || lane.hidden) {
			// Found no lane or lane without a valid render index
			// Skip rendering to avoid crashes
			return;
		}

		const layer = record.layer;
		const ry = getLaneY(lane.renderIndex);
		const color = getGroupRecordColor(record.group)!;

		const rt = ry + LANE_PADDING + layer * LANE_HEIGHT;
		const rb = ry + LANE_HEIGHT + layer * LANE_HEIGHT;

		const bounds = {
			l: rxs,
			r: rxs + rw,
			t: rt,
			b: rb,
			recordReference: record,
			laneReference: lane
		};

		recordBounds.push(bounds);

		// Draw shape
		ctx.rc.beginPath();
		ctx.rc.roundRect(rxs, rt, rw, LANE_HEIGHT - LANE_PADDING * 2, 3);
		ctx.rc.stroke();
		ctx.rc.fillStyle = color;
		ctx.rc.fill();
		ctx.rc.closePath();

		// Draw header shape
		ctx.rc.beginPath();
		ctx.rc.roundRect(rxs, rt, rw, LANE_PADDING * 3.5, 3);
		ctx.rc.fillStyle = color;
		ctx.rc.fill();
		ctx.rc.closePath();

		// Draw group
		ctx.rc.textBaseline = 'middle';
		ctx.rc.textAlign = 'center';
		ctx.rc.fillStyle = shadeColor(color, -50);
		ctx.rc.fillText(textEllideIfNeeded(ctx, record.group, rw), rxs + rw / 2, rt + LANE_PADDING * 2);

		// Draw info labels
		ctx.rc.textBaseline = 'middle';
		ctx.rc.textAlign = 'center';
		ctx.rc.fillStyle = shadeColor(color, -50);
		if(extraw) {
			ctx.rc.fillText(textEllideIfNeeded(ctx, '\u{1F50E}\u{FE0F}', rw * 0.4), rxe - LANE_ML, rt + LANE_PADDING * 2);
		}

		// Draw name
		ctx.rc.textBaseline = 'middle';
		ctx.rc.textAlign = 'left';
		ctx.rc.fillStyle = shadeColor(color, -90);
		ctx.rc.fillText(
			textEllideIfNeeded(ctx, '\u{1F3F7}\u{FE0F}' + record.name, rw - LANE_ML),
			rxs + LANE_ML / 2,
			rt + LANE_PADDING * 2 + (LANE_HEIGHT - LANE_PADDING * 3) / 3
		);

		// Draw span
		ctx.rc.textBaseline = 'middle';
		ctx.rc.textAlign = 'left';
		ctx.rc.fillStyle = shadeColor(color, -90);
		ctx.rc.fillText(
			textEllideIfNeeded(
				ctx,
				'\u{26A1}\u{FE0F}' + calculateTimeDelta(record) +
				'\u{23F1}\u{FE0F}' + formatTime(record.timestamp.begin * 1e6),
				rw - LANE_ML
			),
			rxs + LANE_ML / 2,
			rt + LANE_PADDING * 2 + 2 * (LANE_HEIGHT - LANE_PADDING * 3) / 3
		);
	}

	function reset() {
		ctx.laneCount = 0;
		ctx.rc.font = '14px monospace';
		ctx.titlesz = computeLaneStartOffset(ctx.rc);
		recordBounds.length = 0;
		laneVisibleCountThisFrame.clear();
	}

	function update(dt: number) {
		if(!shouldStop()) {
			ctx.viewStart += dt;
			ctx.viewEnd += dt;
		}

		// Every frame update records
		for(let i = records.length - 1; i >= 0; i--) {
			if(
				(records[i].timestamp.begin.valueOf() < performance.now() - RECORD_LIFETIME_MS) ||
				(i >= RECORD_MAX_SIZE)
			) 
			{
				const lane = getRecordLane(records[i]);
				lane!.layers[records[i].layer] = lane!.layers[records[i].layer].filter(x => x.traceid !== records[i].traceid);
				records.splice(i, 1);
			}
		}
	}

	function jumpToNow() {
		const now = serverSyncedTime();
		ctx.viewStart = now - 10_000;
		ctx.viewEnd = now;
	}

	function draw() {
		reset();

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

		// Draw now line
		drawNowLine();
	}

	function drawIfStopped() {
		if(shouldStop()) {
			draw();
		}
	}

	function resize() {
		canvas.width = wrap.clientWidth;
		canvas.height = height();
		drawIfStopped();
	}

	function mousePos(event: MouseEvent) {
		const rect = canvas.getBoundingClientRect();
		const x = event.clientX - rect.left;
		const y = event.clientY - rect.top;
		return { x, y };
	}

	function recordMutateSysCids(record: TraceRecord) {
		return systemAliasCids.has(record.clientid) ? systemAliasCids.get(record.clientid)! : record.clientid;
	}

	async function addRecord(record: TraceRecord) {
		const addInternal = () => {
			record.clientid = recordMutateSysCids(record);
			record.layer = getLaneFirstAvailableLayer(getRecordLane(record)!);
			records.push(record);
			lanes.get(record.clientid)!.layers[record.layer].push(record);
			resize();
		};

		if(!addRecordWaiting.has(record.clientid)) {
			const rdb = new MxRdb();
			const namekey = '/system/backends/' + record.clientid.toString(16) + '/name';
			addRecordWaiting.set(record.clientid, new Promise<boolean>(async (resolve) => {
				if(await rdb.exists(namekey)) {
					lanes.set(record.clientid, {
						name: await rdb.read(namekey),
						currentLayer: 0,
						layers: [],
						hidden: false
					});
				}
				else {
					systemAliasCids.set(record.clientid, 0x0n);
					if(!lanes.has(0x0n)) {
						lanes.set(0x0n, { name: 'System', currentLayer: 0, layers: [], hidden: true });
					}
				}

				addInternal();
				resolve(true);
			}));
		}
		else {
			await addRecordWaiting.get(record.clientid);
			addInternal();
		}
	}

	function intersectRecordBound(mpos: { x: number, y: number }, bound: TraceRecordBounds) {
		return mpos.x >= bound.l && mpos.x <= bound.r && mpos.y >= bound.t && mpos.y <= bound.b;
	}

	function getRecordGroupAndName(fid: number) {
		return internedStrings !== undefined ? (internedStrings.get(fid)?.split(':') ?? ['Error', '<ISNF>']) : ['Error', '<ISNF>'];
	}

	function nanosToNumber(value: BigInt) {
		const ms = value.valueOf() / 1_000_000n;
		const left = value.valueOf() % 1_000_000n;
		return Number(ms) + Number(left) / 1e6;
	}

	function readRecordsEventBuffer(data: Uint8Array) {
		// MEMO:
		// struct alignas(8) TrxRecord
		// {
		// std::uint64_t _self_rid;
		// std::uint64_t _self_cid;
		// std::uint64_t _trigger_rid;
		// std::uint64_t _trigger_cid;
		// std::int64_t  _timestamp;
		// TrxTag        _tags;
		// std::uint8_t  _padding[3];
		// TrxFuncId	 _fid;
		// };

		const view = new DataView(data.buffer);
		let offset = 0;
		const size = view.getBigUint64(offset, true); offset += 8;

		let overlap = false;

		for(let i = 0; i < size; i++) {
			const rid  = view.getBigUint64(offset, true); offset += 8;
			const cid  = view.getBigUint64(offset, true); offset += 8;
			const trid = view.getBigUint64(offset, true); offset += 8;
			const tcid = view.getBigUint64(offset, true); offset += 8;

			const ts   = view.getBigInt64(offset, true); offset += 8;

			const tags = view.getUint8(offset); offset += 4; // with +3 bytes padding for the layout (8B align)
			const fid  = view.getUint32(offset,  true); offset += 4;
			
			if(timeOffset === undefined) {
				timeOffset = ts / 1_000_000n; // To ms
				perfStartTime = performance.now();
				jumpToNow();
			}

			if(tags & 1) {
				// record start
				const [group, name] = getRecordGroupAndName(fid);
				const record = {
					name: name,
					group: group,
					timestamp: {
						begin: nanosToNumber(ts - (timeOffset.valueOf() * 1_000_000n)),
						end: nanosToNumber(ts - (timeOffset.valueOf() * 1_000_000n) + 1n)
					},
					traceid: rid,
					clientid: cid,
					complete: false,
					layer: -1
				};

				incompleteRecords.set(rid, record);
				setTimeout(() => addRecord(record), 0);

				// If we add another record this frame, it will overlap
				// NOTE: (César) This is not neccessarily true if we are running with prop mode
				overlap = true;
			}
			else {
				// record end

				const markRecordComplete = () => {
					irecord.complete = true;
				};

				const irecord = incompleteRecords.get(rid)!;
				irecord.timestamp.end = nanosToNumber(ts - (timeOffset.valueOf() * 1_000_000n));
				if(ctx.renderMode === 'prop') {
					markRecordComplete();
				}
				else {
					const span = irecord.timestamp.end - irecord.timestamp.begin;
					const ttw = RECORD_MIN_W_MS - span;
					if(ttw > 0) {
						setTimeout(markRecordComplete, ttw);
					}
				}
				incompleteRecords.delete(rid);
			}
		}
	}

	let tickLast = performance.now();
	function tick(now: number) {
		const dt = (now - tickLast);
		tickLast = now;

		update(dt);
		draw();
		requestAnimationFrame(tick);
	}

	onMount(() => {
		// Get interned strings
		MxWebsocket.instance.rpc_call('mulex::TrxGetInternedMap', [], 'generic').then((value: MxGenericType) => {
			const data = value.unpack(['uint32', 'str128']);
			internedStrings = new Map<number, string>(data);
		});

		// Trace record events
		MxWebsocket.instance.subscribe('mxtrace::record', (data: Uint8Array) => readRecordsEventBuffer(data));

		// Rendering
		// Compute lane name max size
		const lctx = canvas.getContext('2d')!;
		lctx.font = '14px monospace';
		ctx = {
			rc: lctx,
			laneCount: 0,

			viewStart: -10_000,
			viewEnd: 0,
			viewOffset: 0,

			titlesz: computeLaneStartOffset(lctx),

			renderMode: 'nonprop'
		};

		// Setup canvas callbacks
		let dragViewStart = 0;
		let dragViewEnd = 0;
		let hoveringRecord: TraceRecord | undefined = undefined;
		let downRecord: TraceRecord | undefined = undefined;
		canvas.addEventListener('mousemove', (event: MouseEvent) => {
			const p = mousePos(event);

			wrap.classList.replace('cursor-pointer', 'cursor-default');
			hoveringRecord = undefined;
			for(const bound of recordBounds) {
				if(intersectRecordBound(p, bound)) {
					wrap.classList.add('cursor-pointer');
					hoveringRecord = bound.recordReference;
					break;
				}
			}

			if(p.x > ctx.titlesz + 2 * LANE_ML) {
				wrap.classList.add('cursor-grab');
			}
			else {
				wrap.classList.remove('cursor-grab');
			}

			if(mouseDragging) {
				const dx = p.x - mouseX;
				const span = dragViewEnd - dragViewStart;
				const ds = (dx / (canvas.width - ctx.titlesz)) * span; 
				ctx.viewStart = dragViewStart - ds;
				ctx.viewEnd = dragViewEnd - ds;
				setShouldStop(true);
				drawIfStopped();
			}
		});

		canvas.addEventListener('mousedown', (event: MouseEvent) => {
			if(event.button !== 0) return;
			const p = mousePos(event);

			downRecord = hoveringRecord;
			if(downRecord) {
				return;
			}

			if(p.x > ctx.titlesz + 2 * LANE_ML) {
				mouseDragging = true;
				mouseX = p.x;
				dragViewStart = ctx.viewStart;
				dragViewEnd = ctx.viewEnd;
				wrap.classList.add('cursor-grabbing');
				return;
			}
		});

		canvas.addEventListener('mouseup', (event: MouseEvent) => {
			if(event.button !== 0) return;
			mouseDragging = false;
			wrap.classList.remove('cursor-grabbing');

			if(hoveringRecord && hoveringRecord === downRecord) {
				props.onRecordSelect && props.onRecordSelect(hoveringRecord, lanes.get(hoveringRecord.clientid)?.name);
				return;
			}
		});

		canvas.addEventListener('mouseleave', (event: MouseEvent) => {
			if(event.button !== 0) return;
			wrap.classList.remove('cursor-grab');
		});

		// Re-draw on div resize
		new ResizeObserver(resize).observe(wrap);

		// render
		requestAnimationFrame(tick);
	});

	return (
		<div>
			<Card title="Options">
				<div class="flex gap-10">
					<div class="grid grid-rows-2 grid-cols-2 gap-2 items-center">
						<div class="text-sm font-bold">Realtime</div>
						<MxDoubleSwitch labelFalse="No" labelTrue="Yes" value={!shouldStop()} onChange={(v: boolean) => {
							setShouldStop(!v);
							if(v) {
								jumpToNow();
							}
						}}/>
						<div class="text-sm font-bold">System Records</div>
						<MxDoubleSwitch labelFalse="No" labelTrue="Yes" value={showSysRecords()} onChange={(v: boolean) => {
							if(lanes.has(0x0n)) {
								setShowSysRecords(v);
								lanes.get(0x0n)!.hidden = !v;
							}
						}}/>
					</div>
				</div>
			</Card>
			<div ref={wrap}>
				<canvas ref={canvas} class='rounded-lg shadow-md hover:shadow-lg'/>
			</div>
		</div>
	);
};
