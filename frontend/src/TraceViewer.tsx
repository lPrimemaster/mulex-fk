import { Component } from "solid-js";
import { DynamicTitle } from "./components/DynamicTitle";
import Sidebar from "./components/Sidebar";
import TraceTimeline from "./components/TraceTimeline";

export const TraceViewer: Component = () => {
	return (
		<div>
			<DynamicTitle title="Tracing"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<TraceTimeline/>
			</div>
		</div>
	);
};
