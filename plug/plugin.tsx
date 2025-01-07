import { Component } from 'solid-js';

// A solid-js component
// Everything solid-js related is valid on this context
const MyPlugin : Component = () => {
	return (
		<div>MyPlugin</div>
	);
};

export const pname = 'MyPlugin'; // Name seen by the mx server instance
export const render = MyPlugin;
// export const icon = () => <svg></svg>; // Custom icon to display at the plugins project page
