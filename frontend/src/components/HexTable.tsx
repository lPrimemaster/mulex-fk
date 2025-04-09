import { Component, For } from "solid-js";

interface MxHexTableProps {
	data: Uint8Array;
	bpr: number;
};

export const MxHexTable : Component<MxHexTableProps> = (props) => {
	const rows = Math.ceil(props.data.length / props.bpr);

	return (
		<table class="table-fixed border border-gray-300 text-sm font-mono">
			<thead>
				<tr>
					<th class="w-20 p-1 text-left bg-gray-100 border-b">Addr</th>
					<For each={[...Array(props.bpr).keys()]}>{(i) =>
						// Fmt like 0A, FF, etc
						<th class="w-12 p-1 text-center bg-gray-100 border-b">{i.toString(16).toUpperCase().padStart(2, "0")}</th>
					}</For>
				</tr>
			</thead>
			<tbody>
				<For each={[...Array(rows).keys()]}>{(i) => {

					const start_addr = i * props.bpr;
					const end_addr = start_addr + props.bpr;
					const row_data = props.data.slice(start_addr, end_addr);

					return (
						<tr class="even:bg-white odd:bg-gray-100 border-t">
							<td class="p-1 text-gray-600 font-bold border-r">
								0x{start_addr.toString(16).toUpperCase().padStart(4, "0")}
							</td>

							<For each={[...Array(props.bpr).keys()]}>{(j) =>
								<td class="p-1 text-center">{row_data[j] === undefined ? '--' : row_data[j].toString(16).toUpperCase().padStart(2, "0")}</td>
							}</For>
						</tr>
					);
				}}</For>
			</tbody>
		</table>
	);
};
