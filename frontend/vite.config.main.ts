import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import solidSvg from 'vite-plugin-solid-svg';
import path from "path";
import fs from 'fs';
import { expandHome } from './dev/helper';
// import { VitePWA } from 'vite-plugin-pwa';
// import devtools from 'solid-devtools/vite';

const manifest = JSON.parse(fs.readFileSync('./../build/manifest.json', 'utf-8'));

// This allows to mimic hotswapping from vite
function devReloadPlugin() {
	const script = `
		<script>
			const es = new EventSource('http://localhost:3000');
			es.onmessage = async () => {
				const cacheKeys = await caches.keys();
				await Promise.all(cacheKeys.map(k => caches.delete(k)));
				window.location.reload();
			};
		</script></body>
	`;

	return {
		name: 'dev-reload',
		transformIndexHtml(html: string) {
			return html.replace('</body>', script);
		}
	};
}

export default defineConfig(({ mode }) => ({
  plugins: [
    /* 
    Uncomment the following line to enable solid-devtools.
    For more info see https://github.com/thetarnav/solid-devtools/tree/main/packages/extension#readme
    */
    // devtools(),
    solidPlugin(),
	solidSvg(),
	...(mode === 'dev' ? [devReloadPlugin()] : [])
	// VitePWA({
	// 	registerType: 'autoUpdate',
	// 	manifest: {
	// 		name: 'Mulex App',
	// 		short_name: 'Mulex App',
	// 		start_url: '/?standalone=1',
	// 		display: 'standalone',
	// 		background_color: '#ffffff',
	// 		theme_color: '#000000'
	// 	}
	// })
  ],
  server: {
    port: 3000,
  },
  build: {
    target: 'esnext',
	outDir: mode === 'dev' ? expandHome(process.env.MX_HOME ?? 'dist') : 'dist',
	rollupOptions: {
		output: {
			entryFileNames: 'index-[hash].js',
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
}));
