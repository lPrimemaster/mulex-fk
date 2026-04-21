import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import solidSvg from 'vite-plugin-solid-svg';
import path from "path";
import fs from 'fs';
import { expandHome } from './dev/helper';
// import devtools from 'solid-devtools/vite';

const manifest = JSON.parse(fs.readFileSync('./../build/manifest.json', 'utf-8'));

export default defineConfig(({ mode }) => ({
  plugins: [
    /* 
    Uncomment the following line to enable solid-devtools.
    For more info see https://github.com/thetarnav/solid-devtools/tree/main/packages/extension#readme
    */
    // devtools(),
    solidPlugin(),
	solidSvg()
  ],
  server: {
    port: 3000,
  },
  build: {
    target: 'esnext',
	outDir: mode === 'dev' ? expandHome(process.env.MX_HOME ?? 'dist') : 'dist',
	rollupOptions: {
		input: {
			login: 'login.html'
		},
		output: {
			entryFileNames: 'login.js',
			chunkFileNames: 'chunks/[name].js',
			assetFileNames: '[name].[ext]'
		}
	},
	emptyOutDir: false
  },
  resolve: {
	  alias: {
		  '~': path.resolve(__dirname, './src')
	  }
  },
  define: {
	  __APP_VERSION__: JSON.stringify(manifest.version),
	  __APP_VNAME__: JSON.stringify(manifest.vname),
	  __APP_GHASH__: JSON.stringify(manifest.hash),
	  __APP_GBRANCH__: JSON.stringify(manifest.branch)
  }
}));
