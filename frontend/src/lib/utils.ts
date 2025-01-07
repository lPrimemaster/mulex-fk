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
