import { clsx } from "clsx"
import type { ClassValue } from 'clsx';
import { twMerge } from "tailwind-merge"

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
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
