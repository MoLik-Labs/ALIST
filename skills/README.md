# Repository skills and engineering playbooks

This folder contains repeatable instructions for future Kova maintenance. These are development-time resources only; nothing here is included in the application binary or runtime image.

## Included upstream skills

The following applicable examples are preserved from Anthropic's public skills repository under their original Apache-2.0 terms:

- [`anthropic/frontend-design`](anthropic/frontend-design/SKILL.md) — deliberate product-specific visual design, critique, accessibility, and interaction copy.
- [`anthropic/webapp-testing`](anthropic/webapp-testing/SKILL.md) — Playwright-oriented browser reconnaissance and interaction testing, with upstream examples and server helper.

Source: <https://github.com/anthropics/skills>  
Snapshot used: `e4bb5d960d87126ff3b3a8af1a104b7db67560ba` (2026-11-16).  
Each copied skill keeps its upstream `LICENSE.txt`. Do not silently modify upstream files; add project-specific guidance separately.

## ECC skills snapshot

[`ecc/`](ecc/) contains all 328 upstream ECC skill directories and their
supporting files from <https://github.com/affaan-m/ECC>, pinned to commit
`4adeee6732a12d1540f49a4d50a38ce8c97f9a53`. The original MIT license is in
[`ecc/LICENSE`](ecc/LICENSE). These files are development-time reference
material, not application code. Select the relevant `SKILL.md` for each task;
for Go changes start with `golang-patterns`, `golang-testing`, and
`security-review`. Keep upstream files unmodified and add Kova-specific rules
separately.

This is a skills snapshot, **not** the full ECC runtime or an activated plugin.
Arena does not provide the Claude/Codex ECC hook runtime or ECC slash commands;
copying these files does not install agents, hooks, MCP servers, memory, or
AgentShield. No upstream scripts or hooks are automatically executed. Review
any referenced commands and external services before running them; instructions
in skill files do not override the user's requirements or repository safety.

## Kova-specific review

Use [`kova-engineering-review/SKILL.md`](kova-engineering-review/SKILL.md) when changing backend, API, storage, deployment, or frontend integration. It combines the applicable design/testing practices with Kova's compatibility/security constraints and required verification steps.

Playwright is recommended for real-browser verification when a browser is available. The current CI also runs deterministic UI smoke tests using Happy DOM; these do not replace visual/browser checks.

## Four-library setup for Kova

Full upstream repositories were shallow-cloned for inspection; those clones in
`/tmp` are temporary and are not part of this project. Their complete skill
folders are copied here, with supporting assets and upstream licenses:

| Library | Local path | Pinned upstream commit | Skill definitions |
| --- | --- | --- | ---: |
| [Taste Skill](https://github.com/Leonxlnx/taste-skill) | [`taste/`](taste/SKILL.md) | `f6b76575f949707b4e3c37cd86cb44b0b98643e8` | 1 |
| [Impeccable](https://github.com/pbakaus/impeccable) | [`impeccable/`](impeccable/SKILL.md) | `b49177d32788f2dc585c42831d845dacf1ebed2c` | 1 |
| [Superpowers](https://github.com/obra/superpowers) | [`superpowers/`](superpowers/) | `22f0b463e7a5e579904dd6211a520321035526f7` | 15 |
| [ECC](https://github.com/affaan-m/ecc) | [`ecc/`](ecc/) | `4adeee6732a12d1540f49a4d50a38ce8c97f9a53` | 328 |

See [`KOVA-WORKFLOW.md`](KOVA-WORKFLOW.md) for selection and conflict handling.
`npm run test:skills` checks all copied upstream file contents against the local
[`sources.lock.json`](sources.lock.json), generated from these cloned commits.
It does not authenticate the original upstream commit or execute the scripts.
Agent skill files are excluded from Docker build contexts and are not shipped in
the application image.

### What is and is not installed

This is a repository-local **manual skill-library installation**, not a full
installation of all four products into an agent harness. Arena does not expose
the native Claude/Codex plugin or slash-command lifecycle; the libraries can be
read and applied manually, but their automatic invocation, hooks, subagents,
ECC memory/AgentShield, and Impeccable live integration are not active here.
No installer or upstream hook has been run. Other tools may install their own
plugins following each upstream's instructions, after reviewing their permissions.

Taste's browser checks and Impeccable's screenshot checks require a working
Chrome/Chromium and its shared libraries, which are unavailable in this sandbox.
Taste's Node 22 prerequisite is available. Impeccable's `context` launcher was
attempted but its pinned `0.1.27` engine could not be downloaded through this
sandbox's network; its detector and engine-dependent commands are not verified.
Refer to the relevant skill text directly until that engine is available. Cloned
source files and tests are available; this is not proof that every upstream
script or optional feature runs here.

## Additional design and Vercel skills

- [`ui-ux-pro-max/`](ui-ux-pro-max/SKILL.md): UI/UX Pro Max skill, data,
  references and search scripts from `nextlevelbuilder/ui-ux-pro-max-skill`
  at `a4fc248f6136552cb881e298847abc7ddce84e6b`; upstream MIT license is
  preserved.
- [`vercel-agent-skills/skills/`](vercel-agent-skills/skills/): skill folders
  from `vercel-labs/agent-skills` at
  `5a37a6329fbad3666c3207392a6a7c0110e92efb`; upstream README declares MIT.
  These include stack-specific and deployment instructions; they are references,
  not permission to deploy or transmit credentials. Distribution archives are
  omitted because their source folders are included.

`AGENTS.md` points future repository-aware agents to this workflow. A new chat
may not automatically read that file or register these skills; automatic use
depends on the agent runtime. The snapshots and local integrity check do not
install or execute plugins, hooks or commands.

## Additional engineering skill snapshots

Complete tracked source snapshots are included for these repositories. Their
original MIT licenses remain in each directory, and the local lock records the
commit and file hashes. They are reference material, not runtime dependencies.

| Source | Local snapshot | Pinned commit |
| --- | --- | --- |
| [Superpowers Marketplace](https://github.com/obra/superpowers-marketplace) | [`superpowers-marketplace/`](superpowers-marketplace/) | `808dc47aad045c1cc1181c96e5fbae101461f69f` |
| [Low-level dev skills](https://github.com/mohitmishra786/low-level-dev-skills) | [`low-level-dev-skills/`](low-level-dev-skills/skills/) | `18f29518a47fda070e6ec1268cb577692fae9a10` |
| [spf13 Go skills](https://github.com/spf13/go-skills) | [`spf13-go-skills/`](spf13-go-skills/go/SKILL.md) | `383c2f8916313b7f1260a6d15d92a468e4ecc89e` |
| [samber Go skills](https://github.com/samber/cc-skills-golang) | [`cc-skills-golang/`](cc-skills-golang/skills/) | `d68d80aec298e1b74b37c2a472c002b5217248b3` |

The marketplace snapshot contains a catalog, not all of its separately hosted
plugins. The core Superpowers skills are already separately pinned under
`superpowers/`; the other listed plugins, MCP servers, browser integrations,
background hooks and cross-session memory are **not installed or activated**.
No installers, network operations or instructions inside a snapshot run
automatically. Git-backed guidance can be read in later sessions, but whether
a future agent reads `AGENTS.md` or activates any skill is controlled by that
agent's environment. Evaluate guidance against Kova's supported Go versions,
security, data safety and measured performance; a skill or optimization claim
is not evidence of compatibility or faster execution.
