import type { Component } from 'solid-js';
import { BadgeLabel, BadgeType } from './ui/badge-label';

const Status: Component<{label: string, value: string, type: BadgeType, class?: string}> = (props) => {
	return (
		<div class="p-0">
			<span class={`text-right mr-2 pl-5 ${props.class ?? ''}`}>{props.label}</span>
			<BadgeLabel type={props.type}>{props.value}</BadgeLabel>
		</div>
	);
};

export default Status;
