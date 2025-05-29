import { Title } from "@solidjs/meta";
import { Component } from "solid-js"
import { gExpname } from "~/lib/globalstate";

export const DynamicTitle : Component<{ title: string }> = (props) => {
	return (
		<Title>{props.title} • {gExpname()}</Title>
	);
};
