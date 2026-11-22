---
layout: home
hero:
  name: Kova
  text: Your files, your workspace.
  tagline: A practical, security-minded guide to deploying your own file workspace.
  actions:
    - theme: brand
      text: Start here
      link: /start
    - theme: alt
      text: Cloudflare guide
      link: /cloudflare
features:
  - title: Pick the right platform
    details: Understand what runs the server and where your data lives before you deploy.
  - title: Keep your data safe
    details: Configure persistent storage, secure access and test backups.
  - title: Follow every step
    details: Learn how to connect a fork, check your deployment and troubleshoot common errors.
---

::: warning Current implementation
The complete Kova server currently runs in a native Linux container. The Cloudflare Worker and Pages projects are **proxies**, not standalone backends. An experimental Cloudflare-native R2 reader exists, but is not a complete backend or production-ready. The Go implementation is experimental.
:::
