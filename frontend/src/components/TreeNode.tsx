import { Component, JSX, Show } from "solid-js";

export const MxTree : Component<{ children: JSX.Element }> = (props) => {
	return <div class="space-y-1">{props.children}</div>;
};

interface MxTreeNodeProps {
	title: string;
	children: JSX.Element;
	open: boolean;
	onClick?: () => void;
};

const MxTreeArrow : Component<{ class?: string }> = (props) => {
	return (
		<div class={props.class}>
			<svg width="16px" height="16px" viewBox="0 0 512 512" version="1.1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink">
				<title>triangle-filled</title>
				<g id="Page-1" stroke="none" stroke-width="1" fill="none" fill-rule="evenodd">
					<g id="drop" fill="#000000" transform="translate(32.000000, 42.666667)">
						<path
							d="M246.312928,5.62892705 C252.927596,9.40873724 258.409564,14.8907053 262.189374,21.5053731 L444.667042,340.84129 C456.358134,361.300701 449.250007,387.363834 428.790595,399.054926 C422.34376,402.738832 415.04715,404.676552 407.622001,404.676552 L42.6666667,404.676552 C19.1025173,404.676552 7.10542736e-15,385.574034 7.10542736e-15,362.009885 C7.10542736e-15,354.584736 1.93772021,347.288125 5.62162594,340.84129 L188.099293,21.5053731 C199.790385,1.04596203 225.853517,-6.06216498 246.312928,5.62892705 Z"
							id="Combined-Shape"
						>
						</path>
					</g>
				</g>
			</svg>
		</div>
	);
};

export const MxTreeNode : Component<MxTreeNodeProps> = (props) => {

	return (
		<div class="border-2 rounded-md">
			<div
				class="flex items-center gap-1 cursor-pointer px-2 py-1 hover:bg-gray-100 rounded-md"
				onClick={() => { if(props.onClick) props.onClick(); }}
			>
				<MxTreeArrow class={props.open ? "rotate-180" : "rotate-90"}/>
				<span>{props.title}</span>
			</div>

			<Show when={props.open}>
				<div class="mx-5 mb-5 pl-2">
					{props.children}
				</div>
			</Show>
		</div>
	);
};
