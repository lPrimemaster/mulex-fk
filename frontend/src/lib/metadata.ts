import { createRoot, createResource } from 'solid-js';
import { MxWebsocket } from './websocket';

async function fetchExperimentName() : Promise<string> {
	const mxsocket = MxWebsocket.instance;
	const name = await mxsocket.rpc_call('mulex::SysGetExperimentName');
	return name.astype('string');
};

const Metadata = () => {
	const [expname] = createResource(fetchExperimentName);
	return {
		expname
	};
};

export default createRoot(Metadata);
