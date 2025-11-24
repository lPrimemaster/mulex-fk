import { Accessor, Component, For, JSXElement, Setter, Show, createContext, createEffect, createSignal, onMount, useContext } from "solid-js";
import { DynamicTitle } from "./components/DynamicTitle";
import Sidebar from "./components/Sidebar";
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "./components/ui/table";
import { BadgeLabel } from "./components/ui/badge-label";
import { SearchBarServerSide } from "./components/SearchBar";
import Card from "./components/Card";
import { Pagination, PaginationEllipsis, PaginationItem, PaginationItems, PaginationNext, PaginationPrevious } from "./components/ui/pagination";
import { MxWebsocket } from "./lib/websocket";
import { MxButton } from "./api";
import AddIcon from './assets/add.svg';
import RemoveIconW from './assets/removeW.svg';
import RemoveIconB from './assets/removeB.svg';
import DeleteIcon from './assets/delete.svg';
import PostIcon from './assets/post.svg';
import CrossIcon from './assets/cross-circle.svg';
import { Checkbox } from "./components/ui/checkbox";
import { SetStoreFunction, createStore } from "solid-js/store";
import { TextField, TextFieldInput, TextFieldLabel } from "./components/ui/text-field";
import { gLoggedUser } from "./lib/globalstate";
import { Avatar, AvatarFallback, AvatarImage } from "./components/ui/avatar";
import { MarkdownDisplay } from "./components/MarkdownEditor";
import { MxColorBadge } from "./components/Badges";
import { MxGenericType } from "./lib/convert";
import { showToast } from "./components/ui/toast";
import { MxPopup } from "./components/Popup";

const POSTS_PER_PAGE = 25;
const COMMENTS_PER_PAGE = 10;

interface LogBookContextType {
	pagePosts: LogBookPostArray;
	setPagePosts: SetStoreFunction<LogBookPostArray>;
	addPost: (id: number, user: string, title: string, date: string, meta: string) => void;
	deletePost: (id: number) => void;
	clearPosts: () => void;
	newPostPage: Accessor<LogBookPageType>;
	setNewPostPage: Setter<LogBookPageType>;
	readPageId: Accessor<number | undefined>;
	setReadPageId: Setter<number | undefined>;
	numPosts: Accessor<number>;
	setNumPosts: Setter<number>;
	query: Accessor<string>;
	setQuery: Setter<string>;
	cache: Map<string, any>;
};

interface LogBookPost {
	id: number;
	selected: boolean;
	body?: string;
	meta?: LogBookFileHandlerArray;
	title: string;
	user: string;
	date: string;
	fileTypes: Array<string>;
};

interface LogBookPostArray {
	items: Array<LogBookPost>;
};

enum LogBookPageType {
	ListPage,
	EditPage,
	ReadPage
};

const LogBookContext = createContext<LogBookContextType>();

const LogBookContextProvider : Component<{ children?: JSXElement }> = (props) => {
	const [pagePosts, setPagePosts] = createStore<LogBookPostArray>({ items: [] });
	const [newPostPage, setNewPostPage] = createSignal<LogBookPageType>(LogBookPageType.ListPage);
	const [readPageId, setReadPageId] = createSignal<number>();
	const [numPosts, setNumPosts] = createSignal<number>(0);
	const [query, setQuery] = createSignal<string>('');
	const cache = new Map<string, any>();

	function clearPosts() {
		setPagePosts("items", []);
	}

	function addPost(id: number, user: string, title: string, date: string, meta: string) {
		const metadataObject = JSON.parse(meta);
		const extensions = new Array<string>();
		for(const file of metadataObject.items) {
			const filename = file.localName as string;
			const ext = filename.split('.').pop();

			if(ext) {
				extensions.push(ext);
			}
		}

		setPagePosts("items", (p) => [...p, {
			id: id,
			selected: false,
			title: title,
			user: user,
			date: date,
			meta: metadataObject,
			fileTypes: [...new Set(extensions)] // Unique file extensions
		}]);
	}

	function deletePost(id: number) {
		setPagePosts("items", (posts) => posts.filter(p => p.id !== id));
	}

	return (
		<LogBookContext.Provider value={{
			pagePosts,
			setPagePosts,
			addPost,
			deletePost,
			clearPosts,

			newPostPage,
			setNewPostPage,

			readPageId,
			setReadPageId,

			numPosts,
			setNumPosts,

			query,
			setQuery,

			cache
		}}>

			{props.children}
		</LogBookContext.Provider>
	);
};

const LogBookAttachmentDisplay : Component<{id: number, files: LogBookFileHandlerArray, onDelete: () => void }> = (props) => {
	const [progress, setProgress] = createSignal<number>(0);
	const [name, setName] = createSignal<string | undefined>('');

	createEffect(() => {
		setName(props.files.items.find(i => i.id === props.id)?.localName);
	});

	createEffect(() => {
		const progress = props.files.items.find(i => i.id === props.id)?.uploadProgress;
		if(progress) {
			setProgress(progress);
		}
	});

	return (
		<div class="flex items-center gap-1
					flex-nowrap p-1 pl-3
					text-sm font-semibold
					bg-gray-600 rounded-md
					text-white place-content-between my-2"
		>
			<div class="flex text-ellipsis overflow-hidden items-center gap-1 flex-nowrap">
				<MxColorBadge color={`${progress() >= 100 ? 'bg-green-500' : 'bg-red-500'}`} size="size-3"/>
				<div class="text-ellipsis overflow-hidden text-nowrap w-full">{name()}</div>
				<Show when={progress() < 100}>
					<div class="min-w-20 border mx-2 border-white border-md bg-transparent">
						<div
							class="inset-0 h-2 bg-gray-100 transition-all duration-300"
							style={{ width: `${progress()}%`}}
						/>
					</div>
				</Show>
			</div>
			<Show when={progress() >= 100}>
				<div class="flex min-w-10 place-content-end">
					{/* @ts-ignore */}
					<DeleteIcon class="size-5 cursor-pointer" onClick={props.onDelete}/>
				</div>
			</Show>
		</div>
	);
};

interface LogBookFileHandler {
	id: number;
	file: File;
	localName: string;
	uploadProgress: number;
	serverHandle?: string;
	serverPath?: string;
	processing: boolean;
};

interface LogBookFileHandlerArray {
	items: Array<LogBookFileHandler>;
};

function modifyBody(text: string, files: LogBookFileHandlerArray) {
	let modifiedBody = text;
	// Replace all file references on text for our uploaded handles.
	for(const file of files.items) {
		if(file.serverPath) {
			modifiedBody = modifiedBody.replaceAll(file.localName, file.serverPath);
		}
	}

	return modifiedBody;
}

const LogBookWritePost : Component = () => {
	const { setNewPostPage } = useContext(LogBookContext) as LogBookContextType;
	const [avatarUrl, setAvatarUrl] = createSignal<string>('');
	const [lineWrap, setLineWrap] = createSignal<boolean>(true);
	const [title, setTitle] = createSignal<string>('');
	const [body, setBody] = createSignal<string>('');
	const [markMode, setMarkMode] = createSignal<number>(1);
	const [markClass, setMarkClass] = createSignal<string>('grid-cols-2 grid-rows-1');
	const [dragging, setDragging] = createSignal<boolean>(false);
	const [files, filesActions] = createStore<LogBookFileHandlerArray>({ items: [] });

	let fid = 0;

	onMount(async () => {
		const path = await MxWebsocket.instance.rpc_call('mulex::PdbUserGetAvatarPath');
		setAvatarUrl(path.astype('string'));
		triggerDraftSaveAttachments(files); // Empty meta for server in case the user does not upload anything
	});

	function deleteFile(id: number) {
		const file = files.items.find(i => i.id === id);
		if(file && file.serverHandle) {
			MxWebsocket.instance.rpc_call('mulex::FdbDeleteFile', [MxGenericType.str512(file.serverHandle)]);
		}
		filesActions("items", items => items.filter(item => item.id !== id));
		triggerDraftSaveAttachments(files);
	}

	async function uploadFile(id: number) {
		const file = files.items.find(i => i.id === id);
		if(!file) return;

		const res = await MxWebsocket.instance.rpc_call('mulex::FdbChunkedUploadStart', [
			MxGenericType.str32(file.file.type)
		]);
		const handle = res.astype('string');
		const fileData = new Uint8Array(await file.file.arrayBuffer());
		const dataSize = fileData.length;
		const chunkSize = 64 * 1024; // Upload chunks of 64KB
		let bytesTransfered = 0;

		filesActions("items", files.items.findIndex(i => i.id === id), "processing", true);
		filesActions("items", files.items.findIndex(i => i.id === id), "uploadProgress", 0);
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

			filesActions("items", files.items.findIndex(i => i.id === id), "uploadProgress", (bytesTransfered / dataSize) * 100);
		}

		const uploadOk = await MxWebsocket.instance.rpc_call('mulex::FdbChunkedUploadEnd', [MxGenericType.str512(handle)]);

		if(!uploadOk.astype('bool'))
		{
			console.error('Failed to end chunk upload. Handle ' + handle + '.');
		}

		filesActions("items", files.items.findIndex(i => i.id === id), "processing", false);
		filesActions("items", files.items.findIndex(i => i.id === id), "serverHandle", handle);

		const serverPath = await MxWebsocket.instance.rpc_call('mulex::FdbGetHandleRelativePath', [MxGenericType.str512(handle)]);
		filesActions("items", files.items.findIndex(i => i.id === id), "serverPath", serverPath.astype('string'));

		triggerDraftSaveAttachments(files);
	}

	function postTabHandle(e: KeyboardEvent) {
		if(e.key === 'Tab') {
			e.preventDefault(); // Disable focus swap on tab

			const target = e.target as HTMLTextAreaElement;
			const start = target.selectionStart;
			const end = target.selectionEnd;

			const content = body();
			const ncontent = content.substring(0, start) + '\t' + content.substring(end);
			setBody(ncontent);

			// Ensure update after DOM changes
			queueMicrotask(() => {
				target.selectionStart = target.selectionEnd = start + 1; // Advance 1 character
			});
		}
	}

	function onDrop(e: DragEvent) {
		e.preventDefault();
		setDragging(false);

		Array.from(e.dataTransfer?.files || []).forEach((file) => {
			filesActions("items", (p) => [...p, { id: ++fid, file: file, uploadProgress: 0, processing: false, localName: file.name }]);
			uploadFile(fid);
		});
	}

	function onDragOver(e: DragEvent) {
		e.preventDefault();
		setDragging(true);
	}

	function onDragLeave(e: DragEvent) {
		e.preventDefault();
		setDragging(false);
	}

	function triggerDraftSaveTitle(text: string) {
		localStorage.setItem('md-post-draft-title', text);
	}

	function triggerDraftSaveBody(text: string) {
		localStorage.setItem('md-post-draft-body', text);
	}

	function triggerDraftSaveAttachments(files: LogBookFileHandlerArray) {
		localStorage.setItem('md-post-draft-files', JSON.stringify(files));
	}

	function loadDraft() {
		const draftt = localStorage.getItem('md-post-draft-title');
		const draftb = localStorage.getItem('md-post-draft-body');
		const draftf = localStorage.getItem('md-post-draft-files');
		if(draftt) {
			setTitle(draftt);
		}

		if(draftb) {
			setBody(draftb);
		}

		if(draftf) {
			const lfiles = JSON.parse(draftf);
			filesActions(lfiles);
			fid = lfiles.items.length;
		}
	}

	function deleteDraft() {
		localStorage.removeItem('md-post-draft-title');
		localStorage.removeItem('md-post-draft-body');

		for(const file of files.items) {
			deleteFile(file.id);
		}
		localStorage.removeItem('md-post-draft-files');
	}

	function post() {
		if(body().length > 0 && title().length > 0) {

			const te = new TextEncoder();
			const bodyRaw = te.encode(body());
			const filesText = localStorage.getItem('md-post-draft-files');
			let metaRaw = new Uint8Array([]);

			if(filesText) {
				metaRaw = new Uint8Array(te.encode(filesText));
			}

			MxWebsocket.instance.rpc_call('mulex::LbkPostCreate', [
				MxGenericType.str512(title()), 			 	// Title
				MxGenericType.makeData(bodyRaw, 'generic'), // Body
				MxGenericType.makeData(metaRaw, 'generic')	// Meta (useful for modifications)
			]).then((res) => {
				if(res.astype('bool')) {
					// NOTE: (Cesar) Cannot call deleteDraft
					// 				 Don't want to delete files
					localStorage.removeItem('md-post-draft-title');
					localStorage.removeItem('md-post-draft-body');
					localStorage.removeItem('md-post-draft-files');
					setNewPostPage(LogBookPageType.ListPage); // Go back to search page
				}
				else {
					// TODO: (Cesar) Error
				}
			});
		}
		else {
			// TODO: (Cesar) Error
		}
	}

	function cycleMarkdownMode() {
		let text: string = '';
		let cls: string = '';

		switch(markMode()) {
			case 0:
				setMarkClass('');
				cls = 'bg-red-200';
				text = 'None';
				break;
			case 1:
				setMarkClass('grid-cols-2 grid-rows-1');
				cls = 'bg-blue-200';
				text = 'Side';
				break;
			case 2:
				setMarkClass('grid-cols-1 grid-rows-2');
				cls = 'bg-green-200';
				text = 'Bellow';
				break;
		}
		return (
			<div class={`select-none py-1 px-2 w-20 cursor-pointer border flex place-content-center text-sm font-semibold rounded-md ${cls}`}>
				{text}
			</div>
		);
	}

	onMount(() => {
		loadDraft();
	});

	createEffect(() => {
		triggerDraftSaveBody(body());
	});

	createEffect(() => {
		triggerDraftSaveTitle(title());
	});

	return (
		<Card title="New Post">
			<div class="flex border inset-shadow-sm rounded-md my-5 place-content-between items-center">
				<TextField class="flex gap-1 items-center w-1/2 px-5 py-2" onChange={setTitle}>
					<TextFieldLabel>Title</TextFieldLabel>
					<TextFieldInput type="text" class="bg-white" value={title()}/>
				</TextField>
				<div class="flex gap-1 items-center py-2">
					<Checkbox checked={lineWrap()} onChange={setLineWrap}/>
					<div>Line wrapping</div>
				</div>
				<div class="flex gap-1 items-center py-2">
					<div>Preview</div>
					<div onClick={() => setMarkMode((markMode() + 1) % 3)}>
						{cycleMarkdownMode()}
					</div>
				</div>
				<div class="flex items-center gap-2 border rounded-md p-2 bg-gray-200 h-10 overflow-visible mx-5">
					<Avatar class="-ml-7">
						<AvatarImage src={avatarUrl()}/>
						<AvatarFallback>{gLoggedUser().toUpperCase().slice(0, 2)}</AvatarFallback>
					</Avatar>
					<div>{gLoggedUser()}</div>
				</div>
			</div>
			<div class={`grid ${markClass()} gap-1`}>
				<div>
					<div class="relative">
						<textarea
							class={
								`w-full min-h-64 h-full focus:outline-none focus:ring-0 rounded-md overflow-hidden resize-none
								 p-2 font-mono shadow-md my-0
								 ${lineWrap() ? ' ' : 'overflow-x-auto whitespace-nowrap '}`}
							onInput={(e) => {
								const el = e.currentTarget;
								el.style.height = "auto";
								el.style.height = el.scrollHeight + "px";
								setBody(el.value);
							}}
							value={body()}
							onKeyDown={postTabHandle}
							style="white-space: pre; tab-size: 4;" // 4 space tabs
							placeholder={`Start typing HTML, Markdown or LaTeX...\nYou can also drag files here...`}
							onDragOver={onDragOver}
							onDragLeave={onDragLeave}
							onDrop={onDrop}
						/>
						<Show when={dragging()}>
							<div
								class="absolute inset-0 flex items-center justify-center
									   rounded-lg font-semibold text-lg pointer-events-none
									   bg-blue-300/50 backdrop-blur-sm"
							>
								<div
									class="flex items-center justify-center w-full h-full p-10 border-4
										   border-dashed border-blue-500 text-blue-900 rounded-lg"
								>
									Drop here
								</div>
							</div>
						</Show>
					</div>
					<Show when={files.items.length > 0}>
						<div class="w-full lg:w-1/2 rounded-md shadow-md bg-transparent">
							<For each={files.items}>{(file) =>
								<LogBookAttachmentDisplay id={file.id} files={files} onDelete={() => deleteFile(file.id)}/>
							}</For>
						</div>
					</Show>
				</div>
				<Show when={markMode() !== 0}>
					<div><MarkdownDisplay content={'# ' + title() + '\n\n' + modifyBody(body(), files)}/></div>
				</Show>
			</div>
			<div class="flex gap-2 mt-2">
				<MxButton onClick={post} class="place-items-center flex gap-1 py-1" type="success">
					{/* @ts-ignore */}
					<PostIcon class="size-5"/>
					<div class="text-xs font-semibold">Post</div>
				</MxButton>
				<MxButton type="error" onClick={() => { setNewPostPage(LogBookPageType.ListPage); deleteDraft(); }}>
					<div class="flex gap-1 items-center">
						{/* @ts-ignore */}
						<DeleteIcon class="size-5"/>
						<div class="text-xs font-semibold">Reject</div>
					</div>
				</MxButton>
			</div>
		</Card>
	);
};

interface LogBookComment {
	author: string;
	body: string;
	date: string;
};

interface LogBookCommentsArray {
	items: Array<LogBookComment>;
};

const LogBookCommentBox : Component<{ comment: LogBookComment }> = (props) => {
	const { cache } = useContext(LogBookContext) as LogBookContextType;
	const [avatarPath, setAvatarPath] = createSignal<string>('');

	onMount(async () => {
		if(cache.has('user_avatar') && cache.get('user_avatar').has(props.comment.author)) {
			const path = cache.get('user_avatar').get(props.comment.author);
			setAvatarPath(path);
		}
		else {
			const res = await MxWebsocket.instance.rpc_call('mulex::PdbUserGetOtherAvatarPath', [MxGenericType.str512(props.comment.author)]);
			const path = res.astype('string');
			setAvatarPath(path);

			if(cache.has('user_avatar')) {
				const map = cache.get('user_avatar');
				map.set(props.comment.author, path);
			}
			else {
				const map =  new Map<string, string>();
				map.set(props.comment.author, path);
				cache.set('user_avatar', map);
			}
		}
	});

	return (
		<div class="bg-gray-100 w-full lg:w-3/4 rounded-md shadow-md">
			<div class="p-5">
				<div class="flex gap-1 items-center mb-2">
					<Avatar>
						<AvatarImage src={avatarPath()}/>
						<AvatarFallback class="bg-gray-300">{props.comment.author.toUpperCase().slice(0, 2)}</AvatarFallback>
					</Avatar>
					<div class="text-md font-semibold">{props.comment.author}</div>
					<div class="text-xs font-normal">{props.comment.date}</div>
				</div>
				<div class="ml-10 text-justify text-wrap">
					{props.comment.body}
				</div>
			</div>
		</div>
	);
};

const LogBookCommentWrite : Component<{ postId: number, onUpdate: () => void }> = (props) => {
	const [content, setContent] = createSignal<string>('');

	function publishComment() {
		const te = new TextEncoder();
		const contentRaw = te.encode(content());

		MxWebsocket.instance.rpc_call('mulex::LbkCommentCreate', [
			MxGenericType.int32(props.postId),
			MxGenericType.makeData(contentRaw, 'generic')
		]).then((res) => {
			if(res.astype('bool')) {
				localStorage.removeItem('comment-draft-body');
				setContent('');
				props.onUpdate();
			}
		});
	}

	return (
		<div class="mb-5">
			<textarea
				class="w-full focus:outline-none focus:ring-0
					   rounded-md overflow-hidden resize-none
					   p-2 font-mono shadow-md my-0"
				onInput={(e) => {
					const el = e.currentTarget;
					el.style.height = "auto";
					el.style.height = el.scrollHeight + "px";
					setContent(el.value);
				}}
				value={content()}
				style="white-space: pre; tab-size: 4;" // 4 space tabs
				placeholder="Add comment..."
			/>
			<div class="flex gap-2 mt-2">
				<MxButton onClick={publishComment} class="place-items-center flex gap-1 py-1" type="success" disabled={content().length === 0}>
					{/* @ts-ignore */}
					<PostIcon class="size-5"/>
					<div class="text-xs font-semibold">Post</div>
				</MxButton>
			</div>
		</div>
	);
};

const LogBookComments : Component<{ postId: number }> = (props) => {
	const [page, setPage] = createSignal<number>(1);
	const [numComments, setNumComments] = createSignal<number>(0);
	const [comments, commentsActions] = createStore<LogBookCommentsArray>({ items: [] });

	createEffect(async () => {
		const nc = await MxWebsocket.instance.rpc_call('mulex::LbkGetNumComments', [MxGenericType.int32(props.postId)]);
		setNumComments(Number(nc.astype('int64')));
	});

	async function updateComments() {
		const res = await MxWebsocket.instance.rpc_call('mulex::LbkGetComments', [
			MxGenericType.int32(props.postId),
			MxGenericType.int64(BigInt(COMMENTS_PER_PAGE)),
			MxGenericType.int64(BigInt(page() - 1))
		], 'generic');

		commentsActions("items", []);
		const comments = res.unpack(['str512', 'cstr', 'str512']);

		for(const comment of comments) {
			const [ user, body, date ] = comment;
			commentsActions("items", (p) => [...p, { author: user, body: body, date: date }]);
		}
	}

	createEffect(async () => {
		if(numComments() > 0) {
			updateComments();
		}
	});

	return (
		<div class="m-0 mt-1 rounded-md shadow-md bg-white">
			<div class="p-5">
				<div class="font-semibold text-md mb-5">Discussion</div>
				<LogBookCommentWrite postId={props.postId} onUpdate={updateComments}/>

				<Show when={numComments() > 0}>
					<div class="flex flex-col gap-5">
						<For each={comments.items}>{(comment: LogBookComment) =>
							<LogBookCommentBox comment={comment}/>
						}</For>
					</div>

					<div class="container mt-5">
						<div class="flex w-full items-center place-content-center">
							<Pagination
								count={Math.ceil(numComments() / COMMENTS_PER_PAGE)}
								fixedItems
								page={page()}
								onPageChange={setPage}
								itemComponent={(p) => <PaginationItem page={p.page}>{p.page}</PaginationItem>}
								ellipsisComponent={() => <PaginationEllipsis/>}
							>
								<PaginationPrevious/>
								<PaginationItems/>
								<PaginationNext/>
							</Pagination>
						</div>
					</div>
				</Show>
			</div>
		</div>
	);
};

const LogBookReadPost : Component = () => {
	const { setNewPostPage, readPageId, pagePosts } = useContext(LogBookContext) as LogBookContextType;
	const [readPage, setReadPage] = createSignal<LogBookPost>();

	onMount(() => {
		const id = readPageId();
		setReadPage(pagePosts.items.find(p => p.id === id));
	});

	return (
		<div class="relative m-2">
			<div
				class="absolute right-0 top-0 cursor-pointer size-8 -mr-4 -mt-4"
				onClick={() => setNewPostPage(LogBookPageType.ListPage)}
			>
				{ /* @ts-ignore */}
				<CrossIcon class="size-8"/>
			</div>
			<Show when={readPage() !== undefined}>
				<MarkdownDisplay content={'# ' + readPage()!.title + '\n\n' + modifyBody(readPage()!.body!, readPage()!.meta!)}/>
				<LogBookComments postId={readPage()!.id}/>
			</Show>
		</div>
	);
};

const LogBookTable : Component<{ page: number }> = (props) => {
	const {
		pagePosts,
		setNewPostPage,
		setPagePosts,
		addPost,
		clearPosts,
		setReadPageId,
		setNumPosts,
		numPosts,
		query
	} = useContext(LogBookContext) as LogBookContextType;

	async function fetchAll(page: number) {
		const res = await MxWebsocket.instance.rpc_call('mulex::LbkGetEntriesPage', [
			MxGenericType.uint64(BigInt(POSTS_PER_PAGE)),
			MxGenericType.uint64(BigInt(page))
		], 'generic');

		clearPosts();
		const posts = res.unpack(['int32', 'str512', 'str512', 'str512', 'bytearray']);
		for(const post of posts) {
			const [ id, user, title, date, meta ] = post;
			const decoder = new TextDecoder();
			addPost(id, user, title, date, decoder.decode(meta));
		}
	}

	async function fetchQuery(page: number) {
		const res = await MxWebsocket.instance.rpc_call('mulex::LbkGetEntriesPageSearch', [
			MxGenericType.str512(query()),
			MxGenericType.uint64(BigInt(POSTS_PER_PAGE)),
			MxGenericType.uint64(BigInt(page))
		], 'generic');

		clearPosts();
		const posts = res.unpack(['int32', 'str512', 'str512', 'str512', 'bytearray']);
		for(const post of posts) {
			const [ id, user, title, date, meta ] = post;
			const decoder = new TextDecoder();
			addPost(id, user, title, date, decoder.decode(meta));
		}
	}

	function clampPage(page: number) {
		return page <= 0 || page > Math.ceil(numPosts() / POSTS_PER_PAGE) ? 0 : (page - 1);
	}

	onMount(async () => {
		clearPosts();
		const res = await MxWebsocket.instance.rpc_call('mulex::LbkGetNumEntriesWithCondition', [MxGenericType.str512('')]);
		setNumPosts(Number(res.astype('int64')));
	});

	createEffect(() => {
		if(query().length > 0) {
			fetchQuery(clampPage(props.page));
		}
		else {
			fetchAll(clampPage(props.page));
		}
	});

	async function onPostClick(post: LogBookPost) {
		if(!post.body) {
			const res = await MxWebsocket.instance.rpc_call('mulex::LbkPostRead', [MxGenericType.int32(post.id)], 'generic');
			if(res.data.length != 0) {
				const mdcontent = res.astype('string');
				setPagePosts("items", pagePosts.items.findIndex(p => p.id === post.id), "body", mdcontent);
			}
			else {
				// ERROR no such post
				return;
			}
		}

		setReadPageId(post.id);
		setNewPostPage(LogBookPageType.ReadPage);
	}

	return (
		<div class="py-0">
			<Table>
				<TableHeader>
					<TableRow>
						<TableHead class="w-5"/>
						<TableHead class="w-1/2 text-nowrap text-ellipsis overflow-hidden">Name</TableHead>
						<TableHead class="text-nowrap">Date</TableHead>
						<TableHead class="text-nowrap">Author</TableHead>
						<TableHead class="text-nowrap">Last Comment</TableHead>
						<TableHead>Files</TableHead>
					</TableRow>
				</TableHeader>
				<TableBody>
					<For each={pagePosts.items}>{(post: LogBookPost, index) =>
						<TableRow
							class={`hover:bg-yellow-100 ${post.selected ? 'bg-blue-100' : 'even:bg-gray-200'}`}
						>
							<TableCell class="py-1">
								<Checkbox checked={post.selected} onChange={
									(checked: boolean) => setPagePosts("items", index(), "selected", checked)
								}/>
							</TableCell>
							<TableCell class="py-1 cursor-pointer" onClick={() => onPostClick(post)}>{post.title}</TableCell>
							<TableCell class="py-1 cursor-pointer" onClick={() => onPostClick(post)}>{post.date}</TableCell>
							<TableCell class="py-1 cursor-pointer" onClick={() => onPostClick(post)}>{post.user}</TableCell>
							<TableCell class="py-1 cursor-pointer" onClick={() => onPostClick(post)}>No comments</TableCell>
							<TableCell class="py-1 cursor-pointer flex gap-1 flex-wrap" onClick={() => onPostClick(post)}>
								<For each={post.fileTypes}>{(ext: string) =>
									<BadgeLabel type="display">{ext}</BadgeLabel>
								}</For>
							</TableCell>
						</TableRow>
					}</For>
				</TableBody>
			</Table>
		</div>
	);
};

const LogBookDeleteCheck : Component<{ open: boolean, ndel: number, changer: Function, deleter: Function }> = (props) => {
	const [open, setOpen] = createSignal<boolean>(false);

	createEffect(() => {
		setOpen(props.open);
	});

	return (
		<MxPopup title="Delete Post(s)" open={open()}>
			<div class="place-items-center">
				<div class="place-content-center justify-center font-semibold text-md">
					You are about to delete {props.ndel} post(s).
				</div>
				<div class="flex place-intems-center justify right my-5 w-1/2 gap-2">
					<MxButton 
						type="error"
						class="w-1/2 h-10 mt-2"
						onClick={() => { setOpen(false); props.changer(false); props.deleter(); }}
					>
						Delete
					</MxButton>
					<MxButton 
						class="w-1/2 h-10 mt-2"
						onClick={() => { setOpen(false); props.changer(false); }}
					>
						Cancel
					</MxButton>
				</div>
			</div>
		</MxPopup>
	);
};

const LogBookControls : Component = () => {
	const [selection, setSelection] = createSignal<boolean>(false);
	const [selectedIds, setSelectedIds] = createSignal<Array<number>>([]);
	const [delPost, setDelPost] = createSignal<boolean>(false);
	const { pagePosts, deletePost, setNewPostPage, setNumPosts, setQuery } = useContext(LogBookContext) as LogBookContextType;

	createEffect(() => {
		const selected = pagePosts.items.filter(item => item.selected).map(item => item.id);
		setSelection(selected.length > 0);
		setSelectedIds(selected);
	});

	async function deletePosts() {
		const fdeletions = [];
		let errorMsg = "";
		let numDel = 0;
		for(const id of selectedIds()) {
			try {
				const res = await MxWebsocket.instance.rpc_call('mulex::LbkPostDelete', [
					MxGenericType.int32(id)
				]);

				if(!res.astype('bool')) {
					fdeletions.push(id);
				}
				else {
					deletePost(id);
					numDel++;
				}
			}
			catch(err) {
				console.log(err);
				fdeletions.push(id);
				errorMsg = "No permission.";
			}
		}

		setSelectedIds(fdeletions);
		setSelection(fdeletions.length > 0);

		console.log(fdeletions);
		
		if(fdeletions.length > 0) {
			// Failed to delete post
			showToast({
				title: 'Failed to delete post(s).',
				description: errorMsg,
				variant: 'error'
			});
		}
		else {
			showToast({
				title: 'Deleted ' + numDel + ' posts.',
				variant: 'success'
			});
		}
	}

	return (
		<div class="mb-5">
			<Card title="">
				<SearchBarServerSide
					queryFunc={async (text: string) => {
						const count = await MxWebsocket.instance.rpc_call('mulex::LbkGetNumEntriesWithCondition', [
							MxGenericType.str512(text)
						]);
						const nposts = Number(count.astype('int64'));
						setNumPosts(nposts);
						setQuery(text);
						return nposts;
					}}
					placeholder="Search for name, date, content or author..."
				/>
				<div class="flex mt-3 gap-3">
					<MxButton onClick={() => setNewPostPage(LogBookPageType.EditPage)} class="place-items-center flex gap-1 py-1" type="success">
						{/* @ts-ignore */}
						<AddIcon class="size-5"/>
						<div class="text-xs font-semibold">New Post</div>
					</MxButton>
					<MxButton onClick={() => setDelPost(true)} class="place-items-center flex gap-1 py-1" type="error" disabled={!selection()}>
						<Show when={selection()}>
							{/* @ts-ignore */}
							<RemoveIconW class="size-5"/>
						</Show>
						<Show when={!selection()}>
							{/* @ts-ignore */}
							<RemoveIconB class="size-5"/>
						</Show>
						<div class="text-xs font-semibold">Delete Post(s)</div>
					</MxButton>
				</div>
				<LogBookDeleteCheck open={delPost()} changer={setDelPost} deleter={deletePosts} ndel={selectedIds().length}/>
			</Card>
		</div>
	);
};

const LogBookBrowse : Component = () => {
	const [page, setPage] = createSignal<number>(1);
	const { numPosts } = useContext(LogBookContext) as LogBookContextType;

	return (
		<div>
			<LogBookControls/>
			<LogBookTable page={page()}/>
			<div class="container mt-5">
				<div class="flex w-full items-center place-content-center">
					<Pagination
						count={Math.ceil(numPosts() / POSTS_PER_PAGE)}
						fixedItems
						page={page()}
						onPageChange={setPage}
						itemComponent={(p) => <PaginationItem page={p.page}>{p.page}</PaginationItem>}
						ellipsisComponent={() => <PaginationEllipsis/>}
					>
						<PaginationPrevious/>
						<PaginationItems/>
						<PaginationNext/>
					</Pagination>
				</div>
			</div>
		</div>
	);
};

const LogBookPage : Component = () => {
	const { newPostPage } = useContext(LogBookContext) as LogBookContextType;
	return (
		<div>
			{/* Read Page */}
			<Show when={newPostPage() === LogBookPageType.ReadPage}>
				<LogBookReadPost/>
			</Show>

			{/* Browse Page */}
			<Show when={newPostPage() === LogBookPageType.ListPage}>
				<LogBookBrowse/>
			</Show>

			{/* New Post Page */}
			<Show when={newPostPage() === LogBookPageType.EditPage}>
				<LogBookWritePost/>
			</Show>
		</div>
	);
};

export const LogBook : Component = () => {

	// TODO: (Cesar)
	// - Message Draft
	// - Links to rdb variables / log them at the time of writing
	// - Modify posts ?
	// - Mentions ?

	return (
		<div>
			<DynamicTitle title="Dashboard"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<LogBookContextProvider>
					<LogBookPage/>
				</LogBookContextProvider>
			</div>
		</div>
	);
};
