import os from 'os';
import path from 'path';

export function expandHome(p: string) {
	return p.startsWith('~') ? path.join(os.homedir(), p.slice(1)) : p;
}
