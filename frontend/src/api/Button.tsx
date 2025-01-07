import { Component, JSXElement } from 'solid-js';

export const MxButton : Component<{ children: JSXElement, type?: string, onClick?: Function, disabled?: boolean, class?: string }> = (props) => {

	let baseatr = `cursor-pointer
				   disabled:cursor-not-allowed
				   rounded-md text-center px-4 py-1 `;

	if(props.type && props.type === 'error') {
		baseatr += 'bg-red-500 border-red-800 text-white hover:bg-red-600 active:bg-red-700 disabled:bg-red-200 disabled:text-gray-500 shadow-md';
	}
	else {
		baseatr += 'bg-gray-200 border-gray-400 hover:bg-gray-300 active:bg-gray-400 disabled:bg-gray-500 disabled:text-gray-600 shadow-md';
	}

	return (
		<button
			class={`${baseatr} ${props.class || ''}`}
			onClick={() => { if(props.onClick) props.onClick(); }}
			disabled={props.disabled ?? false}
		>
			{props.children}
		</button>
	);
};
