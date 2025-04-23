import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import path from 'path';
import pkg from './package.json';
import fg from 'fast-glob';

// Glob files from entry/ into the entrypoint
const entryFiles = fg.sync('src/entry/*.tsx').reduce((entries, file) => {
	const name = path.parse(file).name;
	entries[name] = path.resolve(__dirname, file);
	return entries;
}, {} as Record<string, string>);

export default defineConfig((_) => { 
	return {
		plugins: [
			solidPlugin()
		],
		define: {
			__APP_VERSION__: JSON.stringify(pkg.version)
		},
		build: {
			target: 'ESNext',
			lib: {
				entry: entryFiles,
				formats: ['es']
			},
			rollupOptions: {
				external: [],
				output: {
					entryFileNames: '[name].js',
					chunkFileNames: 'chunks/[name]-[hash].js',
					manualChunks: undefined
				}
			},
			outDir: './dist'
		},
		resolve: {
			alias: {
				'~': path.resolve(__dirname, './src')
			}
		}
	};
});
