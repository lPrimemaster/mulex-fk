import { Component, createEffect, createSignal, For } from "solid-js";

interface MxHexTableProps {
	data: Uint8Array;
	bpr: number;
};

export const MxHexTable : Component<MxHexTableProps> = (props) => {
	const [data, setData] = createSignal<Array<Uint8Array>>([]);

	function make_chunks(data: Uint8Array, chunkSize: number): Array<Uint8Array> {
		const chunks = new Array<Uint8Array>();
		for(let i = 0; i < data.length; i += chunkSize) {
			chunks.push(data.slice(i, i + chunkSize));
		}
		return chunks;
	}

	createEffect(() => {
		setData(make_chunks(props.data, props.bpr));
	});

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
				<For each={data()}>{(arr, i) => {
					const start_addr = i() * props.bpr;

					return (
						<tr class="even:bg-white odd:bg-gray-100 border-t">
							<td class="p-1 text-gray-600 font-bold border-r">
								0x{start_addr.toString(16).toUpperCase().padStart(4, "0")}
							</td>

							<For each={[...Array(props.bpr).keys()]}>{(j) =>
								<td class="p-1 text-center">{arr[j] === undefined ? '--' : arr[j].toString(16).toUpperCase().padStart(2, "0")}</td>
							}</For>
						</tr>
					);
				}}</For>
			</tbody>
		</table>
	);
};
