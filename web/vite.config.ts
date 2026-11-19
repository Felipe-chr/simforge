import tailwindcss from '@tailwindcss/vite'
import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'

import { simforgeCli } from './server/simforge-cli.ts'

// https://vite.dev/config/
export default defineConfig({
  plugins: [
    react(),
    tailwindcss(),
    // Ponte com o núcleo C++ (`simforge-cli api`). Sem o executável, a UI cai
    // no MockTransport automaticamente.
    simforgeCli({ project: 'examples/blink-led.sforge' }),
  ],
  server: {
    port: 5173,
    strictPort: false,
  },
  build: {
    target: 'es2022',
    sourcemap: true,
  },
})
