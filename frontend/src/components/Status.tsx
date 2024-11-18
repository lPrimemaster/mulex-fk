import type { Component } from 'solid-js';
import { BadgeLabel, BadgeType } from './ui/badge-label';

const Status: Component<{label: string, value: string, type: BadgeType}> = (props) => {
	return (
		<div class="flex p-2">
			<span class="text-right mx-5">{props.label}</span>
			<BadgeLabel type={props.type}>{props.value}</BadgeLabel>
		</div>
	);
};

export default Status;
