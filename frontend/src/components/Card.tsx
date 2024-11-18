import type { Component, JSXElement } from 'solid-js';

const Card: Component<{title: string, children?: JSXElement}> = (props) => {
	return (
		<div class="bg-gray-100 p-5 py-2 rounded-md shadow-md hover:shadow-lg">
			<h1 class="text-md text-center font-medium">{props.title}</h1>
			<div>{props.children}</div>
		</div>
	);
};

export default Card;
