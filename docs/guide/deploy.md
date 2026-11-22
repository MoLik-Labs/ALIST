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
