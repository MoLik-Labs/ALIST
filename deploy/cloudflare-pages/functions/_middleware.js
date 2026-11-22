// Pages Functions adapter for the same hardened streaming proxy as Workers.
// It intentionally intercepts every request, including the root and assets.
import { handleRequest } from '../../cloudflare-worker/src/index.js';

export function onRequest(context) {
  return handleRequest(context.request, context.env);
}
