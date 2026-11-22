# Kova Cloudflare Pages proxy

This Pages project is an **edge proxy**, not an independent Kova server. It
uses the same proxy logic as the Worker and requires a separately deployed,
persistent Kova origin. The middleware handles every request; the static HTML
is only an error page if Functions do not run. It does not store accounts or files.

## Connect a GitHub repository

1. Deploy a Kova origin on a container platform with persistent storage; check
   its HTTPS `/ping` and administrator login first.
2. Create a **Pages** project connected to your repository or fork. Select its
   deployment branch. Use root directory `deploy/cloudflare-pages`, build
   command `exit 0`, and build output directory `public`. Do not select `web`
   as the output directory: it is only the UI and has no backend.
3. In Pages settings, add the **runtime** secret `KOVA_ORIGIN` as the origin
   URL (for example `https://example.onrender.com`, with no credentials, path,
   query or fragment). Set it separately for production and any previews that
   should connect to an origin. Redeploy if necessary for the change to apply.
4. Visit the Pages URL and verify login and file operations. Without a valid
   secret the proxy returns 503; verify Functions were deployed if you see the
   static setup page. Do not expose production secrets or production data to
   untrusted preview builds.

Pages and Workers have request-size/time limits; this proxy does not cache
private responses. Restrict origin access before enabling `ALIST_TRUST_PROXY`.
The Cloudflare deploy button supports Workers, not Pages. Git integration,
Functions, account permissions and actual uploads have not yet been verified
on a live Pages account. See [the Worker guide](../cloudflare-worker/README.md)
for security and origin requirements.
