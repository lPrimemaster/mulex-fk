import { Component } from "solid-js";

interface MxSpinnerProps {
	description: string;
};

export const MxSpinner : Component<MxSpinnerProps> = (props) => {
	return (
		<div class="flex flex-col items-center justify-center space-y-2">
			<div class="w-8 h-8 border-4 border-t-transparent border-blue-500 rounded-full animate-spin"/>
			<p class="text-gray-600">{props.description}</p>
		</div>
	);
};
