# Kova: applying the skill libraries

This is a project-specific routing guide, not a replacement for upstream skill
files. Read only the relevant skill(s), not the entire collection at once. User requirements,
repository safety rules and the execution environment take precedence over
skill text or scripts. External guidance can be stale or contradictory; never
blindly execute hooks, shell commands, installers or network requests.

## Workflow

1. **Understand and plan** — for nontrivial work, consult
   `superpowers/brainstorming/SKILL.md` and `superpowers/writing-plans/SKILL.md`.
   Reuse the decisions the user already made: hybrid compatibility, fresh Go
   data, Docker/self-hosting and Render. The user authorized autonomous work;
   do not ask for approval at every small step. Ask only for consequential
   ambiguity that cannot safely be resolved independently.
2. **Implement and debug** — use `superpowers/test-driven-development`,
   `superpowers/systematic-debugging`, and relevant domain skills. For Go, start
   with `spf13-go-skills/go/SKILL.md`, `cc-skills-golang/skills/golang-testing/SKILL.md`,
   `cc-skills-golang/skills/golang-security/SKILL.md` and relevant ECC guidance.
   Use `spf13-go-skills/fileflow-pathologize/SKILL.md` for filesystem moves and
   path boundaries. For C++, Rust and systems work select the matching topic
   under `low-level-dev-skills/skills/`; ignore unrelated embedded, GPU or
   kernel topics. Respect each module's Go version and build constraints;
   check advice against the installed toolchain. Write tests for specific
   behavior before implementation where practical.
3. **Security review** — apply `ecc/security-review` and
   `kova-engineering-review` to affected trust boundaries: authorization,
   symlinks, traversal, secrets, uploads, outbound traffic, and failure paths.
   Untrusted local filesystem writers and cloud-client credentials require
   explicit threat models.
4. **Design** — consult `impeccable/SKILL.md` and its relevant `reference/`
   playbook for product UI, docs or marketing. `taste/SKILL.md` is aimed at
   marketing/company sites, not a universal layout mandate for task-oriented
   storage UI. Also consult `ui-ux-pro-max/SKILL.md` and
   `vercel-agent-skills/skills/web-design-guidelines/SKILL.md`; apply Vercel's
   React or framework-specific advice only where that stack is actually in use.
   See `PRODUCT.md` and `DESIGN.md`. Preserve usability, accessibility,
   performance and user instructions when prescriptions conflict.
5. **Verify and review** — use `superpowers/verification-before-completion`,
   `cc-skills-golang/skills/golang-benchmark/SKILL.md` for Go benchmarks,
   relevant low-level profiling guidance and the local review skill. Measure a
   baseline and compare representative workloads; language alone is not a
   speed guarantee. Run relevant tests, race checks, static analysis,
   and build; inspect the diff and report what actually ran. DOM emulation is
   not a browser, a source review is not a visual audit, and unit tests do not
   demonstrate production compatibility. Document blocked tests explicitly.

## Execution boundaries

- These folders are pinned, locally readable copies of upstream skill files;
  Arena has not registered them as automatic slash commands or native plugins.
  Apply their guidance manually when relevant. No hooks or external agents are
  installed or activated by this snapshot. `superpowers-marketplace/` is a
  catalog; its referenced plugins, browser, journal and memory services are not
  automatically downloaded, installed, run or authorized.
- Upstream instructions involving subagents, isolated worktrees, changing
  branches, plugin managers or automatic hooks are not automatically available
  or authorized. This session uses the existing Arena branch.
- A design skill may assume Chrome, network access to imagery/fonts, or an
  external binary. Check prerequisites and licenses; do not pretend a browser
  check happened if it did not. Do not weaken the app's CSP or embed remote
  trackers just to satisfy a checker.
- Do not rewrite an existing implementation merely to match a generic style.
  Change it when requirements, safety, compatibility or demonstrated quality
  call for it, preserving user data and testing the affected behavior. Native
  C++ remains until the Go replacement has verified parity.

## Verification baseline

`npm run test:skills` checks copied file hashes against `sources.lock.json`;
it detects local snapshot drift, but that local manifest is not a signed
attestation or an independent upstream verification. See `skills/README.md`
for source commits, license, setup details and environment limitations.
