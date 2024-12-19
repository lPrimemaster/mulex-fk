import { Component, createSignal } from 'solid-js';
import { GaugeVertical } from './GaugeVertical';
import { MxWebsocket } from '../lib/websocket';
import { MxGenericType } from '../lib/convert';
import { MxRdb } from '~/lib/rdb';

export const ResourcePanel : Component = () => {
	const [cpuUsage, setCpuUsage] = createSignal<number>(0);
	const [memTotal, setMemTotal] = createSignal<number>(0);
	const [memUsed, setMemUsed] = createSignal<number>(0);
	MxWebsocket.instance.on_connection_change((conn: boolean) => {
		if(conn) {
			const rdb = new MxRdb();
			rdb.watch('/system/metrics/cpu_usage', (_: string, value: MxGenericType) => {
				setCpuUsage(value.astype('float64'));
			});

			rdb.watch('/system/metrics/mem_used', (_: string, value: MxGenericType) => {
				setMemUsed(Number(value.astype('uint64')) / (1024 * 1024 * 1024));
			});

			MxWebsocket.instance.rpc_call('mulex::RdbReadValueDirect', [MxGenericType.str512('/system/metrics/mem_total')], 'generic').then((res) => {
				// To GB
				setMemTotal(Number(res.astype('uint64')) / (1024 * 1024 * 1024));
			});
		}
	});
	return (
		<div class="flex gap-5 place-content-center">
			<GaugeVertical height="200px" width="120px" title="CPU Usage" min={0} max={100} value={cpuUsage()} displayMode="percentage"/>
			{/*<GaugeVertical height="200px" width="120px" title="CPU Temp" min={0} max={100} value={91} displayMode="absolute" units="&#176;C"/>*/}
			<GaugeVertical height="200px" width="120px" title="RAM" min={0} max={memTotal()} value={memUsed()} displayMode="absolute" units="GB"/>
			{/*<GaugeVertical height="200px" width="120px" title="Disk IO" min={0} max={100} value={91} displayMode="percentage" units="GB"/>*/}
		</div>
	);
};
