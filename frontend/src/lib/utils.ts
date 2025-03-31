import { clsx } from "clsx"
import type { ClassValue } from 'clsx';
import { twMerge } from "tailwind-merge"

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
}

export function cssColorToRGB(color: string) {
	const t = document.createElement('div');
	t.style.color = color;
	document.body.appendChild(t);
	const rgb = getComputedStyle(t).color;
	document.body.removeChild(t);
	return rgb;
}

export async function check_condition(cond: () => boolean, delay: number = 100) {
	while(!cond()) await new Promise(resolve => setTimeout(resolve, delay));
}

export function timestamp_tohms(ms: number): string {
	let sec = Math.trunc(ms / 1000);
	if(sec < 60) return sec.toString() + 's';

	let min = Math.trunc(sec / 60);
	if(min < 60) return min.toString() + 'm ' + (sec % 60).toString() + 's';

	let hour = Math.trunc(min / 60);
	if(hour < 24) return hour.toString() + 'h ' + (min % 60).toString() + 'm ' + (sec % 60).toString() + 's';

	let day = Math.trunc(hour / 24);
	return day.toString() + 'd ' + (hour % 24).toString() + 'h ' + (min % 60).toString() + 'm ' + (sec % 60).toString() + 's';
}

export function timestamp_tolocaltime(ts: number): string {
	return (new Date(Number(ts))).toTimeString().split(' ')[0];
}

export function timestamp_tolocaldatetime(ts: number): string {
	const date = new Date(Number(ts));
	return date.toTimeString().split(' ')[0] + ' ' + date.toDateString();
}

export function scroll_to_element(element: HTMLDivElement | undefined) {
	element?.scrollIntoView({ behavior: 'smooth', block: 'end' });
}

export function array_chunkify<T>(items: Array<T>, chunkSize: number): Array<Array<T>> {
	const chunks = new Array<Array<T>>();
	for(let i = 0; i < items.length; i += chunkSize) {
		chunks.push(items.slice(i, i + chunkSize));
	}
	return chunks;
}

export function event_io_extract(io: bigint) {
	return {
		'r': Number((io >> BigInt(32)) & BigInt(0xFFFFFFFF)),
		'w': Number(io & BigInt(0xFFFFFFFF))
	};
}

function bps_get_multiplier(value: number): number {
	if(Math.trunc(value / 1024) > 0) {
		if(Math.trunc(value / 1048576) > 0) {
			if(Math.trunc(value / 1073741824) > 0) {
				return 3;
			}
			return 2;
		}
		return 1;
	}
	return 0;
}

function bps_get_suffix(multiplier: number, bits: boolean): string {
	if(multiplier == 3) return bits ? 'Gbps' : 'GB/s';
	if(multiplier == 2) return bits ? 'Mbps' : 'MB/s';
	if(multiplier == 1) return bits ? 'kbps' : 'kB/s';
	if(multiplier == 0) return bits ?  'bps' :  'B/s';
	return '';
}

export function bps_to_string(value: number, bits: boolean = true) {
	if(bits) {
		value *= 8;
	}

	const multiplier = bps_get_multiplier(value);
	const suffix = bps_get_suffix(multiplier, bits);
	const str = (value / (1024 ** multiplier)).toFixed(1);

	return str + ' ' + suffix;
}
