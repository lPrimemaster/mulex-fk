import { Component } from 'solid-js';

// A solid-js component
// Everything solid-js related is valid on this context
const MyPlugin : Component = () => {
	return (
		<div>MyPlugin</div>
	);
};

// MANDATORY: Name seen by the mx server instance
export const pname = 'MyPlugin';

// MANDATORY: The render component
export const render = MyPlugin;

//  OPTIONAL: The current plugin version
export const version = '1.0.0';

//  OPTIONAL: Description text to display at the plugins project page
export const description = 'The mxfk default sample plugin. Displays basic functionality from the mxfk typescript API.';

//  OPTIONAL: Author name to display at the plugins project page
export const author = 'César Godinho';

//  OPTIONAL: Custom icon to display at the plugins project page
// export const icon = () => <svg></svg>;
