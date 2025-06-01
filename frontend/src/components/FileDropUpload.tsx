import { Component, For, Show, createEffect, createSignal } from "solid-js";
import { createMapStore } from "~/lib/rmap";
import FileIcon from '~/assets/file.svg';
import DeleteIcon from '~/assets/cross-circle.svg';
import { MxWebsocket } from "~/lib/websocket";
import { MxGenericType } from "~/lib/convert";

interface MxFileDropUploadProps {
	allowedExtensions?: Array<string>;
	limit?: number;
	onUploadCompleted?: (handles: Array<string>) => void;
};

export const MxFileDropUpload : Component<MxFileDropUploadProps> = (props) => {
	const [dragging, setDragging] = createSignal<boolean>(false);
	const [files, filesActions] = createMapStore<string, File>(new Map<string, File>());
	const [allowFiles, setAllowFiles] = createSignal<boolean>(true);
	const [errorMsg, setErrorMsg] = createSignal<string>('');

	createEffect(() => {
		setAllowFiles(files.data.size < (props.limit || 1000));
	});

	async function uploadFiles() {
		let handles = new Array<string>();
		Array.from(files.data.values()).forEach(async (file) => {
			const res = await MxWebsocket.instance.rpc_call('mulex::FdbChunkedUploadStart', [
				MxGenericType.str32(file.type)
			]);
			const handle = res.astype('string');
			const fileData = new Uint8Array(await file.arrayBuffer());
			const chunkSize = 64 * 1024; // Upload chunks of 64KB

			for(let i = 0; i < fileData.length; i += chunkSize)
			{
				const chunk = fileData.slice(i, i + chunkSize);
				const chunkOk = await MxWebsocket.instance.rpc_call('mulex::FdbChunkedUploadSend', [
					MxGenericType.str512(handle),
					MxGenericType.makeData(chunk, 'generic')
				]);

				if(!chunkOk.astype('bool'))
				{
					console.error('Failed to upload chunk. Handle ' + handle + '.');
				}
			}

			const uploadOk = await MxWebsocket.instance.rpc_call('mulex::FdbChunkedUploadEnd', [MxGenericType.str512(handle)]);

			if(!uploadOk.astype('bool'))
			{
				console.error('Failed to end chunk upload. Handle ' + handle + '.');
			}

			handles.push(handle);
		});

		if(props.onUploadCompleted) {
			props.onUploadCompleted(handles);
		}
	}

	function onDrop(e: DragEvent) {
		e.preventDefault();
		setDragging(false);

		if(!allowFiles()) return;
		
		Array.from(e.dataTransfer?.files || []).forEach((file, idx) => {
			if((props.limit || 1000) > idx) {
				if(props.allowedExtensions) {
					const ext = file.name.split('.').pop();
					if(ext && props.allowedExtensions.includes(ext.toLowerCase())) {
						filesActions.add_or_modify(file.name, file);
					}
					else {
						setErrorMsg('Cannot upload ' + file.name + ' (invalid extension).');
						setTimeout(() => setErrorMsg(''), 5000);
					}
				}
				else {
					filesActions.add_or_modify(file.name, file);
				}
			}
		});
	}

	function dropFileElement() {
		return (
			<>
				<p>Drop files here</p>
				<Show when={props.allowedExtensions}>
					<p>(.{props.allowedExtensions!.reduce((pext, ext) => pext + ' .' + ext)})</p>
				</Show>
			</>
		);
	}

	return (
		<div class="w-full max-w-md p-4">
			<div
				class={`border-2 border-dashed p-8 rounded-xl text-center transition ${
					allowFiles() ?
						dragging() ? 'bg-blue-100 border-blue-400' : 'bg-transparent border-gray-300' :
						dragging() ? 'bg-red-100 border-red-400' : 'bg-transparent border-red-300'
				}`}
				onDrop={onDrop}
				onDragOver={(e: DragEvent) => { e.preventDefault(); setDragging(true); }}
				onDragLeave={() => setDragging(false)}
			>
				<p>
					{
						allowFiles() ? 
							dragging() ? 'Drop to upload' : dropFileElement() :
							dragging() ? 'Cannot upload more files' : `Limit reached (${props.limit || 1000} files)`
					}
				</p>
			</div>

			<Show when={errorMsg() != ''}>
				<div
					class="flex items-center text-center justify-center
						   text-sm font-semibold bg-red-300 border
						   border-red-900 p-3 w-full rounded-lg my-4"
				>
					{errorMsg()}
				</div>
			</Show>

			<Show when={files.data.size > 0}>
				<button
					class="w-full bg-blue-600 text-white px-8 py-2 mt-4 rounded-lg hover:bg-blue-700 transition"
					onClick={uploadFiles}
				>
					Upload Files
				</button>
				<div class="flex gap-2 flex-wrap w-full mt-4 place-content-center">
					<For each={Array.from(files.data.values())}>{(file) => {
						return (
							<>
								<div
									class="relative cursor-pointer"
									onClick={() => filesActions.remove(file.name)}
								>
									<div
										class="absolute inset-0 size-20 bg-red-300/50 z-5 border-2
											   border-red-900 rounded-lg backdrop-blur-sm
											   opacity-0 hover:opacity-100 transition-opacity
											   flex items-center justify-center"
									>
										{/* @ts-ignore */}
										<DeleteIcon class="size-16"/>
									</div>
									<div
										class="size-20 rounded-lg border border-black place-items-center py-4 px-2 z-1"
									>
										{/* @ts-ignore */}
										<FileIcon class="size-10 fill-black"/>
										<div class="w-full text-center text-ellipsis whitespace-nowrap overflow-hidden">
											{file.name}
										</div>
									</div>
								</div>
							</>
						);
					}}</For>
				</div>
			</Show>
		</div>
	);
};
