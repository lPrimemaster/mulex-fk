import http from 'http';
import chokidar from 'chokidar';
import os from 'os';
import path from 'path';

const OUT_DIR = (process.env.MX_HOME ?? 'dist').replace(/^~/, os.homedir());
const clients = [];

// SSE server on port 3000
http.createServer((req, res) => {
	res.writeHead(200, {
		'Content-Type': 'text/event-stream',
		'Cache-Control': 'no-cache',
		'Access-Control-Allow-Origin': '*',
		Connection: 'keep-alive',
	});
	clients.push(res);
	req.on('close', () => clients.splice(clients.indexOf(res), 1));
}).listen(3000);

// Watch the output dir for changes
let lastSent = 0;
chokidar.watch([path.join(OUT_DIR, 'index.html')], { ignoreInitial: true }).on('change', () => {
	const now = Date.now();
	if(now - lastSent < 5000) {
		console.log('[reload] Debounce triggered...');
		return;
	}

	lastSent = now;
	console.log('[reload] Build changed, notifying clients...');
	clients.forEach(r => r.write('data: reload\n\n'));
});
