import { defineConfig, loadEnv } from 'vite';
import solidPlugin from 'vite-plugin-solid';

export default defineConfig((_) => { 
	return {
		plugins: [solidPlugin()],
		build: {
			lib: {
				entry: './src/index.tsx',
				formats: ['es'],
				name: 'mulex-api',
				fileName: (format, name) => `${name}.${format}.js`
			}
		}
	};
});
