import type { Component, JSXElement } from 'solid-js';

const Card: Component<{title: string, children?: JSXElement}> = (props) => {
	return (
		<div class="bg-gray-100 p-5 mb-5 rounded-md shadow-md hover:shadow-lg" style="break-inside: avoid-column;">
			<h1 class="text-md text-center font-medium">{props.title}</h1>
			<div>{props.children}</div>
		</div>
	);
};

export default Card;
