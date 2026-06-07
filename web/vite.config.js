import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

// The device shell page (components/pixgate/web_server.cpp) loads the bundle with fixed names
// from the GitHub Pages base URL: `<spa_base>/pixgate.js` and `<spa_base>/pixgate.css`. So we
// emit exactly those two files at the publish root and inline everything into the one JS entry
// (no hashed chunk names the shell couldn't predict).
export default defineConfig({
  plugins: [svelte()],
  // Pages serves this project under /PixGate/. Adjust if you rename the repo.
  base: '/PixGate/',
  build: {
    target: 'es2020',
    cssCodeSplit: false,
    rollupOptions: {
      output: {
        inlineDynamicImports: true,
        entryFileNames: 'pixgate.js',
        assetFileNames: (asset) => {
          const name = asset.name || '';
          if (name.endsWith('.css')) return 'pixgate.css';
          return 'assets/[name][extname]';
        },
      },
    },
  },
});
