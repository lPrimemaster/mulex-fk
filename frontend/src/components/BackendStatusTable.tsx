import { Component, For, Show, createSignal } from 'solid-js';
import { BadgeLabel } from './ui/badge-label';
import { BadgeDelta } from './ui/badge-delta';
import { timestamp_tohms, bps_to_string, calculate_text_color_yiq } from '../lib/utils';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from './ui/table';
import { gBackends as backends } from '../lib/globalstate';

const BackendStatusTable: Component = () => {

	const [time, setTime] = createSignal(Date.now() as number);
	setInterval(() => setTime(Date.now() as number), 1000);

	return (
		<div>
			<Table>
				<TableHeader>
					<TableRow>
						<TableHead>Name</TableHead>
						<TableHead>Status</TableHead>
						<TableHead>Host</TableHead>
						<TableHead>Data</TableHead>
						<TableHead>Uptime</TableHead>
					</TableRow>
				</TableHeader>
				<TableBody>
					<For each={Object.keys(backends)}>{(clientid: string) =>
						<TableRow>
							<TableCell class="p-0">{backends[clientid].name}</TableCell>
							<TableCell class="p-0">
								<Show when={backends[clientid].connected && backends[clientid].user_status === 'None'}>
									<BadgeLabel type="success">Connected</BadgeLabel>
								</Show>
								<Show when={backends[clientid].connected && backends[clientid].user_status !== 'None'}>
									<div
										class="
										inline-flex items-center border
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
									{
									//<BadgeLabel type="success">{backends[clientid].user_status}</BadgeLabel>
									}
								</Show>
								<Show when={!backends[clientid].connected}>
									<BadgeLabel type="error">Disconnected</BadgeLabel>
								</Show>
							</TableCell>
							<TableCell class="p-0">
								{backends[clientid].host}
							</TableCell>
							<TableCell class="p-0 flex">
								<span class="p-0 w-28"><BadgeDelta deltaType="increase">{bps_to_string(backends[clientid].evt_upload_speed, false)}</BadgeDelta></span>
								<span class="p-0 w-28"><BadgeDelta deltaType="decrease">{bps_to_string(backends[clientid].evt_download_speed, false)}</BadgeDelta></span>
							</TableCell>
							<TableCell class="p-0">
								<Show when={backends[clientid].connected}>
									{timestamp_tohms(time() - backends[clientid].uptime)}
								</Show>
								<Show when={!backends[clientid].connected}>
									<span class="text-gray-500">Disconnected</span>
								</Show>
							</TableCell>
						</TableRow>
					}</For>
				</TableBody>
			</Table>
		</div>
	);
};

export default BackendStatusTable;
