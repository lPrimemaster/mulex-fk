import { Component, For, Show, createEffect, createSignal } from "solid-js";
import { createMapStore } from "~/lib/rmap";
import FileIcon from '~/assets/file.svg';
import DeleteIcon from '~/assets/cross-circle.svg';
import { MxWebsocket } from "~/lib/websocket";
import { MxGenericType } from "~/lib/convert";
import { Progress, ProgressLabel, ProgressValueLabel } from "./ui/progress";
import { bps_to_string, bytes_to_string } from "~/lib/utils";

interface MxFileDropUploadProps {
	allowedExtensions?: Array<string>;
	limit?: number;
	maxSize?: number;
	onUploadCompleted?: (handles: Array<string>) => void;
	onUploadProgress?: (progress: number) => void;
};

export const MxFileDropUpload : Component<MxFileDropUploadProps> = (props) => {
	const [dragging, setDragging] = createSignal<boolean>(false);
	const [files, filesActions] = createMapStore<string, File>(new Map<string, File>());
	const [allowFiles, setAllowFiles] = createSignal<boolean>(true);
	const [errorMsg, setErrorMsg] = createSignal<string>('');
	const [progress, setProgress] = createSignal<number>(0);
	const [maxProgress, setMaxProgress] = createSignal<number>(0);
	const [uploading, setUploading] = createSignal<boolean>(false);
	const [currFilename, setCurrFilename] = createSignal<string>('');
	const [calculatedBps, setCalculatedBps] = createSignal<number>(0);

	createEffect(() => {
		setAllowFiles(files.data.size < (props.limit || 1000));
	});

	async function uploadFiles() {
		setUploading(true);

		let handles = new Array<string>();
		let bytesTransfered = 0;

		const interval = setInterval(() => {
			setCalculatedBps(bytesTransfered * 2);
			bytesTransfered = 0;
		}, 500);

		for(const file of files.data.values()) {
			const res = await MxWebsocket.instance.rpc_call('mulex::FdbChunkedUploadStart', [
				MxGenericType.str32(file.type),
				MxGenericType.str32('')
			]);
			const handle = res.astype('string');
			const fileData = new Uint8Array(await file.arrayBuffer());
			const dataSize = fileData.length;
			const chunkSize = 64 * 1024; // Upload chunks of 64KB
			const noChunks = Math.ceil(dataSize / chunkSize);
			let currChunk = 0;

			setProgress(0);
			setMaxProgress(noChunks);
			setCurrFilename(file.name);

			for(let i = 0; i < dataSize; i += chunkSize)
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

				bytesTransfered += chunk.length;

				setProgress(++currChunk);
				if(props.onUploadProgress) {
					props.onUploadProgress(currChunk / noChunks);
				}
			}

			const uploadOk = await MxWebsocket.instance.rpc_call('mulex::FdbChunkedUploadEnd', [MxGenericType.str512(handle)]);

			if(!uploadOk.astype('bool'))
			{
				console.error('Failed to end chunk upload. Handle ' + handle + '.');
			}

			handles.push(handle);
		}

		if(props.onUploadCompleted) {
			props.onUploadCompleted(handles);
		}

		clearInterval(interval);
		setUploading(false);
	}

	function checkExtension(file: File) {
		if(props.allowedExtensions) {
			const ext = file.name.split('.').pop();
			if(ext && props.allowedExtensions.includes(ext.toLowerCase())) {
				return true;
			}
			else {
				setErrorMsg('Cannot upload ' + file.name + ' (invalid extension).');
				setTimeout(() => setErrorMsg(''), 5000);
				return false;
			}
		}

		return true;
	}

	function checkSize(file: File) {
		if(props.maxSize) {
			if(file.size <= props.maxSize) {
				return true;
			}
			else {
				setErrorMsg('Cannot upload ' + file.name + ' (too large).');
				setTimeout(() => setErrorMsg(''), 5000);
				return false;
			}
		}

		return true;
	}

	function onDrop(e: DragEvent) {
		e.preventDefault();
		setDragging(false);

		if(!allowFiles()) return;
		
		Array.from(e.dataTransfer?.files || []).forEach((file, idx) => {
			if((props.limit || 1000) > idx) {
				if(checkExtension(file) && checkSize(file)) {
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
				<Show when={props.maxSize}>
					<p>(Maximum size {bytes_to_string(props.maxSize!)})</p>
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

			<Show when={uploading()}>
				<Progress
					value={progress()}
					minValue={0}
					maxValue={maxProgress()}
					getValueLabel={() => bps_to_string(calculatedBps(), false)}
					class="w-full p-3 my-4"
				>
					<div class="flex justify-between">
						<ProgressLabel>Uploading '{currFilename()}'...</ProgressLabel>
						<ProgressValueLabel/>
					</div>
				</Progress>
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
