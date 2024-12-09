import { Component, For, useContext } from 'solid-js';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from './ui/table';
// import { MxWebsocket } from '../lib/websocket';
// import { MxGenericType } from '~/lib/convert';
import { SearchBarContext, SearchBarContextType } from './SearchBar';

export const RdbTable: Component<{ onRowClick?: Function }> = (props) => {
	const { filteredItems, setSelectedItem } = useContext(SearchBarContext) as SearchBarContextType;

	function getName(item: string) {
		return item.split('/').pop();
	}

	function getPath(item: string) {
		return item.split('/').slice(0, -1).join('/');
	}

	// function getType(item: string) {
	// 	MxWebsocket.instance.rpc_call('mulex::RdbListKeyTypes', [], 'generic').then((res) => {
	// 		MxGenericType.typeFromTypeid(res.astype('uint8'));
	// 	});
	// }

	return (
		<>
			<div class="py-0">
				<Table>
					<TableHeader>
						<TableRow>
							<TableHead>Name</TableHead>
							<TableHead>Path</TableHead>
							{/*<TableHead>Type</TableHead>*/}
							{/*<TableHead>Array</TableHead>*/}
						</TableRow>
					</TableHeader>
					<TableBody>
						<For each={filteredItems()}>{(item) =>
							<TableRow class="hover:bg-yellow-100 cursor-pointer even:bg-gray-200" onClick={() => {
								setSelectedItem(item);
								if(props.onRowClick) {
									props.onRowClick(item);
								}
							}}>
								<TableCell class="py-1">{getName(item)}</TableCell>
								<TableCell class="py-1">{getPath(item)}</TableCell>
							</TableRow>
						}</For>
					</TableBody>
				</Table>
			</div>
		</>
	);
};

