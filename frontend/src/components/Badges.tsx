import { Component, createEffect, createSignal, Show } from "solid-js";

export const MxCaptureBadge : Component<{capture: boolean}> = (props) => {
	return (
		<span class="relative flex h-3 w-3">
			<Show when={props.capture}>
				<span class="animate-ping absolute inline-flex h-full w-full rounded-full bg-red-400 opacity-75"></span>
			</Show>
			<span class={`relative inline-flex rounded-full h-3 w-3 ${props.capture ? 'bg-red-500' : 'bg-gray-500'}`}></span>
		</span>
	);
};

export const MxTickBadge : Component<{on: boolean}> = (props) => {
	const [blink, setBlink] = createSignal<boolean>(false);
	let timeout: NodeJS.Timeout;
	
	createEffect(() => {
		if(props.on) {
			setBlink(true);
			clearTimeout(timeout);
			timeout = setTimeout(() => setBlink(false), 50);
		}
	});

	return (
		<span class="relative flex h-3 w-3">
			<span class={`relative inline-flex rounded-full h-3 w-3 ${blink() ? 'bg-green-500' : 'bg-gray-500'}`}></span>
		</span>
	);
};
