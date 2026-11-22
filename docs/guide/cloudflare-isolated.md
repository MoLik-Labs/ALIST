# Cloudflare: deploy an isolated experiment

::: danger Read before starting
This is an **experimental test**, not a complete Kova deployment. Do not use production accounts, files, tokens or databases. Do not overwrite `kova-inc` or another existing Worker. Drivers, sign-in, sharing, security and real-cloud testing are incomplete.
:::

## Prerequisites

- A Cloudflare account on **Workers Free** and a separate **empty test D1 database**. This example uses `kova-cloudflare`; substitute your own name if different. Check the plan and limits in [D1 pricing](https://developers.cloudflare.com/d1/platform/pricing/). Free-plan queries can stop when limits are reached; do not upgrade to follow this guide.
- Node.js and npm on your own computer and the HTTPS clone URL of **your own fork**. Run the commands on your computer, not in the dashboard.
- A new Worker name that does not already exist in your account, for example `kova-cloudflare-isolated-test`. This is separate from the database name. Check Workers & Pages first: deploying the same name can replace an existing Worker.
- Your fork's `CLOUDFLARE` branch must contain these changes. If they are still in a PR, review and merge it into the correct branch first. This is not the `BETA` branch or the upstream repository.

**R2 is not needed** to test the interface and D1 setup. If R2 asks you to subscribe or add payment details, stop; there is no need to upgrade or provide a card for this step.

## 1. Get the right code from your fork

In GitHub open **your fork → Code** and copy its HTTPS URL. Replace `YOUR_FORK_CLONE_URL` below with that URL:

```sh
git clone --branch CLOUDFLARE YOUR_FORK_CLONE_URL kova-cf-test
cd kova-cf-test
npm ci --ignore-scripts --prefix deploy/cloudflare-worker
```

If your branch has a different name, substitute its exact name. If Git cannot find the branch or the example file is missing in the next step, stop and check your fork/PR; do not silently switch to upstream.

## 2. Make an isolated configuration

```sh
cp deploy/cloudflare-native/wrangler.isolated.example.toml deploy/cloudflare-native/wrangler.isolated.toml
```

Open the copied `deploy/cloudflare-native/wrangler.isolated.toml` and replace **three** values:

1. `name`: the **new** test Worker name you checked, not `kova-inc`.
2. `database_name`: `kova-cloudflare` or your actual test D1 name.
3. `database_id`: the Database ID on the Cloudflare Dashboard page for the **correct test D1**.

Do not change `binding = "DB"`, `ASSETS`, `main`, `migrations_dir`, or add R2 for this step. Do not put secrets, passwords, tokens or private keys in this file. The copied configuration is excluded by `.gitignore`; do not remove that rule or commit the file. If you have multiple Cloudflare accounts, check the name and account containing the D1 database before proceeding.

## 3. Check without deploying

From the `kova-cf-test` repository root:

```sh
./deploy/cloudflare-worker/node_modules/.bin/wrangler deploy --dry-run --config deploy/cloudflare-native/wrangler.isolated.toml
```

Check the output shows Worker code, `env.ASSETS` and `env.DB` for the test database. `--dry-run` does **not** create a Worker, run migrations, or prove live D1 or OAuth works. If it fails, stop and share the error with sensitive values removed; never share tokens or callback URLs containing authorization codes.

## 4. Prepare the empty D1 database

Only after confirming the database is an **empty dedicated test database**, sign in with `./deploy/cloudflare-worker/node_modules/.bin/wrangler login` and check the account. List migrations:

```sh
./deploy/cloudflare-worker/node_modules/.bin/wrangler d1 migrations list DB --remote --config deploy/cloudflare-native/wrangler.isolated.toml
```

`--remote` addresses real Cloudflare resources. Recheck the database ID and the files under `deploy/cloudflare-native/migrations/` before applying. When certain, run:

```sh
./deploy/cloudflare-worker/node_modules/.bin/wrangler d1 migrations apply DB --remote --config deploy/cloudflare-native/wrangler.isolated.toml
```

The command may prompt for confirmation; review before accepting. This makes real changes, not a dry-run. Never run it against important data. Migration `0009` deletes pending Google connection attempts from earlier experimental versions. Run `migrations list` again to verify. On failure, stop and retain redacted logs; do not run arbitrary SQL or delete the database.

## 5. Deploy the separate test Worker

After checking the new Worker name and D1 binding in the config again, run:

```sh
./deploy/cloudflare-worker/node_modules/.bin/wrangler deploy --config deploy/cloudflare-native/wrangler.isolated.toml
```

This is a **real deployment** to the Worker named in the config. Open the URL Wrangler provides and check the interface, `GET /ping`, CSS and JS. This template only enables the UI and liveness endpoint; a successful `/ping` does **not** prove database, sign-in or provider functionality. Login/provider features remain disabled, so sign-in will not work yet; do not add secrets or flags just to bypass an error. Keep `kova-inc` automatic builds disabled while testing.

## 6. What is not safely automated yet

Sign-in and test account connections additionally require Google OAuth, a correct same-host HTTPS callback, a vault key, and a verified Google `sub` mapped to a D1 user. There is no safe provisioning UI, key recovery, production rate limiting, or real Google/Dropbox test yet; consequently this is not an instruction to enable it for real users. Do not use email as `sub`, insert unverified identities, or share secrets/tokens in chat. See `deploy/cloudflare-native/README.md` in **your own fork and branch** for experimental flags; wait for further safety and testing work before enabling them.

Upstream README deploy buttons may point to the upstream repository rather than your fork. These steps deliberately use your own clone and a separate Worker. When reporting errors share only the failing step, redacted message, test Worker name and database name; never share keys, passwords, tokens or callback URLs containing codes.
