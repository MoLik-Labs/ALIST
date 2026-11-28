// Keep documentation commands independent of the caller's shell and disable
// Astro telemetry; the built documentation has no analytics integration.
import { spawnSync } from 'node:child_process';
const args = process.argv.slice(2);
const result = spawnSync(process.execPath, ['node_modules/astro/bin/astro.mjs', ...args], {
  stdio: 'inherit', env: { ...process.env, ASTRO_TELEMETRY_DISABLED: '1' },
});
if (result.error) throw result.error;
process.exit(result.status ?? 1);
