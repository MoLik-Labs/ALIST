# Render and Docker

## Render Blueprint

1. Fork the repository if you want to control updates; connect that repository in Render and create a Blueprint from its root `render.yaml`. Alternatively use the upstream README button.
2. Review the paid plan, region and 1 GB disk before approving. Render builds the native Docker image and mounts the disk at `/opt/alist/data`.
3. Render generates `ALIST_ADMIN_PASSWORD`. Find it in the service's environment settings; treat it as a secret. It is used only when the database is first created. Changing the variable later does not reset an existing password.
4. Open the HTTPS service URL and sign in as `admin`. Configure your storage mounts. Verify `/ping`, login, file operations and a restart. Confirm that the disk is attached and data remains.
5. Enable two-factor authentication, back up data and test restoration. Automatic deployment is disabled by the Blueprint; review and deploy updates deliberately.

A disk-backed SQLite deployment is intended for **one server instance**. Multiple independent replicas do not share consistent state. Persistent disks and compute may incur charges.

## Docker Compose

On a Linux host with Docker and Compose installed, replace `YOUR_GITHUB_CLONE_URL` with the HTTPS clone URL of **your fork** (GitHub → your repository → Code):

```sh
git clone YOUR_GITHUB_CLONE_URL kova
cd kova
docker compose up -d --build
```

Open `http://localhost:5244` on the host. Obtain the initial generated password from protected container logs, or set a strong `ALIST_ADMIN_PASSWORD` before first initialization. Do not share logs containing generated credentials. `./data` holds SQLite, keys and configuration; back it up. Protect network access and terminate HTTPS before exposing the service.

For Railway, Fly.io and other hosts, see `docs/platform-deployment.md` in your chosen repository and branch. Cloud platform flows have not all been verified on real accounts.

## Render Free: disposable test without a Blueprint

::: danger Not durable or production-ready
Free services cannot attach a persistent disk. SQLite, accounts, signing keys, settings and local files can disappear on spin-down, restart or redeploy. External drive files are separate, but connections can be lost. Do not connect real accounts or use real data. Free services spin down after inactivity, can take about a minute to wake and have usage limits: see [Render Free](https://render.com/docs/free). The current SQLite backend does not support durable external PostgreSQL, so this is not a durable free deployment.
:::

1. Merge and push these changes to a branch of **your public repository**. Render cannot see unmerged local changes.
2. Dashboard → **New → Web Service** → public Git repository; enter your own repository URL, select the correct branch, leave Root Directory blank. Authorize GitHub if Render asks. Do not use the upstream README button for your fork.
3. Select **Docker**, `./Dockerfile`, context `.` if asked, and **Free**; do not add a disk. Disable automatic deploys if you prefer. In Advanced set health check `/ping` and `KOVA_DISPOSABLE_DEMO=true` to warn in logs.
4. Before the first deploy, set a strong random `ALIST_ADMIN_PASSWORD` in Environment; do not commit or share it. It initializes a new database only, including after the ephemeral database disappears. Without it, the initial generated password is printed in sensitive logs.
5. Review the confirmation for Free only, no disk or paid resources. Do not proceed if Render requests a payment method you do not want to provide; account verification requirements are Render's decision. After deploy open the HTTPS URL, test `/ping`, sign in as `admin`, and use only dummy data. This does not verify persistence.

Alternatively create a **new, separate** Blueprint using Blueprint Path `deploy/render/render.free-demo.yaml`; it generates the password in Environment and creates no disk. Never switch an existing service or Blueprint containing valuable data to this demo: changing or deleting resources can cause data loss. The root `render.yaml` remains the paid disk-backed configuration. Neither flow has been verified on a real Render account.
