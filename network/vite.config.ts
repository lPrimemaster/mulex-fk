// NOTE: (Cesar)
// This is the server configuration file to build user plugins
// It has nothing to do with the frontend main app build stage
// It will simply be copied to the cache location typically '.mxcache'
import { defineConfig, loadEnv } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import path from 'path';

export default defineConfig(({ mode }) => { 
	const env = loadEnv(mode, process.cwd(), '');
	return {
		plugins: [solidPlugin()],
		build: {
			lib: {
				entry: path.resolve(__dirname, env.ENTRY_FILE),
				formats: ['es'],
				fileName: (format, name) => `mxp.${format}.${name}.js`
			},
			outDir: './mxp.comp'
		}
	};
});
