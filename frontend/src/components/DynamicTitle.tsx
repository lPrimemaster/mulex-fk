import { Title } from "@solidjs/meta";
import { Component } from "solid-js"
import metadata from "~/lib/metadata";

export const DynamicTitle : Component<{ title: string }> = (props) => {
	return (
		<Title>{props.title} • {metadata.expname()}</Title>
	);
};
