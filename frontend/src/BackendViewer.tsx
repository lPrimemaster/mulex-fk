import { Component, For, Show, createEffect, createSignal, useContext } from "solid-js";
import { DynamicTitle } from "./components/DynamicTitle";
import Sidebar from "./components/Sidebar";
import { gBackends as backends } from './lib/globalstate';
import Card from "./components/Card";
import { MxColorBadge } from "./components/Badges";
import { BadgeLabel } from "./components/ui/badge-label";

import InputIcon from './assets/input_type.svg';
import HostIcon from './assets/host.svg';
import TimerIcon from './assets/timer.svg';
import PlayIcon from './assets/play.svg';
import StopIcon from './assets/stop.svg';
import LogsIcon from './assets/logs.svg';

import { bps_to_string, calculate_text_color_yiq, timestamp_tohms } from "./lib/utils";
import { MxInlineGraph } from "./components/InlineGraph";
import { BadgeDelta } from "./components/ui/badge-delta";
import { MxButton } from "./api";
import { MxWebsocket } from "./lib/websocket";
import { MxGenericType } from "./lib/convert";
import { showToast } from "./components/ui/toast";
import { MxLogContext, LogContext, MxLog } from "./components/LogTable";
import { Dialog, DialogContentWide, DialogDescription, DialogHeader, DialogTitle, DialogTrigger } from "./components/ui/dialog";

const BackendLogViewer: Component<{cid: string}> = (props) => {
	const { blogs } = useContext(LogContext) as MxLogContext;
	let ref!: HTMLDivElement;

	createEffect(() => {
		if(blogs[props.cid].length > 0) {
			ref.scrollTo({ top: ref.scrollHeight });
		}
	});

	return (
		<div class="border bg-white rounded-md md:h-40 lg:h-64 overflow-y-scroll" ref={ref}>
			<For each={blogs[props.cid]}>{(log: MxLog) => {
				const bg_color = (log.type == 'error') ? 'bg-red-100' : (log.type == 'warn') ? 'bg-yellow-100' : '';
				const class_style = 'px-1 py-0.5 border-b border-gray-300 ' + bg_color;
				const label_type = (log.type == 'error') ? 'error' : (log.type == 'warn') ? 'warning' : 'display';
				return (
					<div class={class_style}>
						<BadgeLabel class="py-0.5 px-1 place-content-center" style="width: 4rem;" type="display">{log.timestamp}</BadgeLabel>
						<BadgeLabel class="py-0.5 px-1 ml-1" type={label_type}>{log.type}</BadgeLabel>
						<span class="ml-1 py-0.5">{log.message}</span>
					</div>
				);
			}}</For>
		</div>
	);
};

export const BackendViewer: Component = () => {

	const [currentLog, setCurrentLog] = createSignal<string>('');
	const [time, setTime] = createSignal(Date.now() as number);
	setInterval(() => setTime(Date.now() as number), 1000);

	function get_error(code: number) {
		return [
			"BACKEND_START_OK",
			"BACKEND_START_FAILED",
			"BACKEND_STOP_OK",
			"BACKEND_STOP_FAILED",
			"NO_SUCH_BACKEND",
			"NO_SUCH_HOST",
			"NO_SUCH_COMMAND",
			"COMMAND_TX_ERROR",
			"COMMAND_TX_TIMEOUT"
		][code];
	}

	function start_backend(cid: BigInt) {
		MxWebsocket.instance.rpc_call('mulex::RexSendStartCommand', [
			MxGenericType.uint64(cid)
		]).then((res) => {
			const r = res.astype('uint8');
			if(r != 0) {
				// Fail
				showToast({
					title: 'Failed to start backend.',
					description: 'Error code: ' + get_error(r),
					variant: 'error'
				});
			}
		});
	}

	function stop_backend(cid: BigInt) {
		MxWebsocket.instance.rpc_call('mulex::RexSendStopCommand', [
			MxGenericType.uint64(cid)
		]).then((res) => {
			const r = res.astype('uint8');
			if(r != 2) {
				// Fail
				showToast({
					title: 'Failed to stop backend.',
					description: 'Error code: ' + get_error(r),
					variant: 'error'
				});
			}
		});
	}
	
	return (
		<div>
			<DynamicTitle title="Backends"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<div class="w-full">
					<For each={Object.keys(backends)}>{(clientid: string) => {
						return (
							<Card title="">
								<div class="flex max-h-32 place-content-between">
									<div class="flex absolute place-content-start items-center -mt-7 -ml-7">
										<MxColorBadge
											color={backends[clientid].connected ? 'bg-green-500' : 'bg-red-500'}
											size="size-5"
											animate={backends[clientid].connected}
										/>
										<Show when={backends[clientid].connected}>
											<BadgeLabel type="success" class="h-5 -ml-2 z-10">Connected</BadgeLabel>
										</Show>
										<Show when={!backends[clientid].connected}>
											<BadgeLabel type="error" class="h-5 -ml-2">Disconnected</BadgeLabel>
										</Show>
										<Show when={backends[clientid].connected && backends[clientid].user_status !== 'None'}>
											<MxColorBadge
												color=""
												class="ml-2"
												size="size-5"
												animate
												style={{
													"background-color": backends[clientid].user_color,
												}}
											/>
											<div
												class="
												h-5 -ml-2 z-9 inline-flex items-center border
												px-2.5 py-0.5 text-xs font-semibold border-transparent
												rounded-md
												"
												style={{
													"background-color": backends[clientid].user_color + "80",
													"color": calculate_text_color_yiq(backends[clientid].user_color, 128)
												}}
											>
												{backends[clientid].user_status}
											</div>
										</Show>
									</div>
									<div class="flex place-content-start flex-wrap">
										<div class="flex place-content-start items-center">
											<div>
												{/* @ts-ignore */}
												<InputIcon class="size-7"/>
											</div>
											<div class="font-bold ml-2 w-40 min-w-40">
												{backends[clientid].name}
											</div>
										</div>
										<div class="flex place-content-start items-center">
											<div>
												{/* @ts-ignore */}
												<HostIcon class="size-7"/>
											</div>
											<div class="font-bold ml-2 w-40 min-w-40">
												{backends[clientid].host}
											</div>
										</div>
										<div class="flex place-content-start items-center">
											<div>
												{/* @ts-ignore */}
												<TimerIcon class="size-7"/>
											</div>
											<div class="font-bold ml-2 w-40 min-w-40">
												<Show when={backends[clientid].connected}>
													{timestamp_tohms(time() - backends[clientid].uptime)}
												</Show>
												<Show when={!backends[clientid].connected}>
													<span class="text-gray-500">Disconnected</span>
												</Show>
											</div>
										</div>
										<div class="flex place-content-start items-center px-5 hidden 2xl:block">
											<div class="flex place-content-center items-center">
												<div class="flex absolute place-content-center items-center font-semibold text-xs text-success-foreground">
													Upstream
													<BadgeDelta deltaType="increase">{bps_to_string(backends[clientid].evt_upload_speed, false)}</BadgeDelta>
												</div>
												<MxInlineGraph
													color="green"
													height={64}
													width={180}
													class="border bg-success rounded-md shadow-md"
													npoints={100}
													values={[]}
												/>
											</div>
										</div>
										<div class="flex place-content-start items-center px-5 hidden 2xl:block">
											<div class="flex place-content-center items-center">
												<div class="flex absolute place-content-center items-center font-semibold text-xs text-error-foreground">
													Downstream
													<BadgeDelta deltaType="decrease">{bps_to_string(backends[clientid].evt_download_speed, false)}</BadgeDelta>
												</div>
												<MxInlineGraph
													color="red"
													height={64}
													width={180}
													class="border bg-error rounded-md shadow-md"
													npoints={100}
													values={[]}
												/>
											</div>
										</div>
										<div class="flex place-content-start items-center px-5">
											<MxButton
												class="mr-2 flex place-content-center"
												disabled={backends[clientid].connected}
												type="success"
												onClick={() => start_backend(BigInt('0x' + clientid))}
											>
												{/* @ts-ignore */}
												<PlayIcon class="size-7 mr-2"/>
												<div class="text-black font-bold">Start</div>
											</MxButton>
											<MxButton
												class="flex place-content-center"
												disabled={!backends[clientid].connected}
												type="error"
												onClick={() => stop_backend(BigInt('0x' + clientid))}
											>
												{/* @ts-ignore */}
												<StopIcon class="size-7 mr-2"/>
												<div class="text-black font-bold">Stop</div>
											</MxButton>
										</div>
										<div class="flex place-content-start items-center px-5">
											<MxButton
												class="mr-2 flex place-content-center"
												onClick={() => setCurrentLog(clientid)}
											>
												{/* @ts-ignore */}
												<LogsIcon class="size-7 mr-2"/>
												<div class="text-black font-bold">View Logs</div>
											</MxButton>
										</div>
									</div>
								</div>
							</Card>
						);
					}}</For>
					<Dialog open={currentLog() !== ''} onOpenChange={(o: boolean) => { if(!o) setCurrentLog(''); }}>
						<DialogTrigger></DialogTrigger>
						<DialogContentWide>
							<DialogHeader>
								<DialogTitle>{`Logs ${currentLog() !== '' ? '(' + backends[currentLog()].name + ')': ''}`}</DialogTitle>
								<DialogDescription>
									Displaying up to the last 100 logs.
								</DialogDescription>
							</DialogHeader>
							<Show when={currentLog() !== ''}>
								<BackendLogViewer cid={currentLog()}/>
							</Show>
						</DialogContentWide>
					</Dialog>
					{
					// <MxPopup
					// 	title={`Logs ${currentLog() !== '' ? '(' + backends[currentLog()].name + ')': ''}`}
					// 	open={currentLog() !== ''}
					// 	onOpenChange={(o: boolean) => { if(!o) setCurrentLog(''); }}
					// >
					// 	<Show when={currentLog() !== ''}>
					// 		<BackendLogViewer cid={currentLog()}/>
					// 	</Show>
					// </MxPopup>
					}
				</div>
			</div>
		</div>
	);
};
