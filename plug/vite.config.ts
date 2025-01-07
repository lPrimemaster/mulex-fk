import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import path from 'path';

export default defineConfig((_) => { 
	return {
		plugins: [
			solidPlugin()
		],
		build: {
			target: 'ESNext',
			lib: {
				entry: './src/plugin.tsx',
				formats: ['es'],
				fileName: (_, name) => `${name}.js`
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
