# Choose a deployment

Kova has two distinct parts: a web interface and a server that handles accounts, files, storage mounts and APIs. Publishing only static HTML does **not** create that server.

| What you want | Current option | Data persistence |
| --- | --- | --- |
| Simple managed server | [Render Blueprint](./deploy) | Paid persistent disk required |
| Your own machine | [Docker Compose](./deploy) | Persist `./data` and back it up |
| Cloudflare hostname | [Worker or Pages proxy](./cloudflare) | A separate persistent server is required |
| Heroku standard dyno | Disposable demo only | Dyno files are ephemeral; accounts and keys can disappear |
| GitHub Pages | This documentation site | Static docs only; it cannot run the Kova API |

The Render button in the repository points to the upstream project. If you fork it, select your fork in the provider dashboard to deploy your own changes. Review prices and permissions before approving resources.

::: danger Back up before upgrading
Keep a consistent backup of the database, signing key and configuration. A disk is not a backup. Remote object storage does not automatically preserve your Kova accounts or settings.
:::
