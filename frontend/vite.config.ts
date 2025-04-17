import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import path from "path";
import fs from 'fs';
import { VitePWA } from 'vite-plugin-pwa';
// import devtools from 'solid-devtools/vite';

const manifest = JSON.parse(fs.readFileSync('./../build/manifest.json', 'utf-8'));

export default defineConfig({
  plugins: [
    /* 
    Uncomment the following line to enable solid-devtools.
    For more info see https://github.com/thetarnav/solid-devtools/tree/main/packages/extension#readme
    */
    // devtools(),
    solidPlugin(),
	VitePWA({
		registerType: 'autoUpdate',
		manifest: {
			name: 'Mulex App',
			short_name: 'Mulex App',
			start_url: '/?standalone=1',
			display: 'standalone',
			background_color: '#ffffff',
			theme_color: '#000000'
		}
	})
  ],
  server: {
    port: 3000,
  },
  build: {
    target: 'esnext',
	rollupOptions: {
		output: {
			entryFileNames: 'index.js',
			chunkFileNames: 'chunks/[name].js',
			assetFileNames: '[name].[ext]'
		}
	}
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
});
