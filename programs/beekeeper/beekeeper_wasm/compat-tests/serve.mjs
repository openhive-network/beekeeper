import { createServer } from 'http';
import { readFile } from 'fs/promises';
import path from 'path';

const PORT = 8090;

const routes = {
  '/new/': path.resolve(import.meta.dirname, '../dist/bundle'),
  '/old/': path.resolve(import.meta.dirname, 'node_modules/@hiveio/beekeeper-old/dist/bundle'),
};

const mimeTypes = {
  '.html': 'text/html',
  '.js':   'application/javascript',
  '.mjs':  'application/javascript',
  '.wasm': 'application/wasm',
  '.json': 'application/json',
};

const server = createServer(async (req, res) => {
  const url = req.url ?? '/';

  // Serve test.html at root
  if (url === '/' || url === '/test.html') {
    const html = await readFile(path.resolve(import.meta.dirname, 'test.html'), 'utf-8');
    res.writeHead(200, {
      'Content-Type': 'text/html',
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
    });
    res.end(html);
    return;
  }

  // Route /new/* and /old/* to their respective directories
  for (const [prefix, baseDir] of Object.entries(routes)) {
    if (url.startsWith(prefix)) {
      const relPath = url.slice(prefix.length);
      const filePath = path.join(baseDir, relPath);
      try {
        const data = await readFile(filePath);
        const ext = path.extname(filePath);
        res.writeHead(200, {
          'Content-Type': mimeTypes[ext] || 'application/octet-stream',
          'Cross-Origin-Opener-Policy': 'same-origin',
          'Cross-Origin-Embedder-Policy': 'require-corp',
        });
        res.end(data);
      } catch {
        res.writeHead(404);
        res.end('Not found');
      }
      return;
    }
  }

  res.writeHead(404);
  res.end('Not found');
});

server.listen(PORT, () => console.log(`Serving on http://localhost:${PORT}`));
