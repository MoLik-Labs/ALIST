import { readFile } from 'node:fs/promises';
import { Window } from 'happy-dom';
import assert from 'node:assert/strict';

const window = new Window({ url: 'https://example.org/' });
window.document.write(await readFile('docs/site/index.html', 'utf8'));
const { document } = window;
window.eval(await readFile('docs/site/app.js', 'utf8'));
for (const link of document.querySelectorAll('a[href^="#"]')) {
  assert.ok(document.getElementById(link.getAttribute('href').slice(1)), `missing target ${link.href}`);
}
const search = document.querySelector('#nav-search');
search.value = 'cloudflare';
search.dispatchEvent(new window.Event('input'));
assert.equal([...document.querySelectorAll('#nav a')].filter(a => !a.hidden).length, 1);
assert.equal(document.querySelector('#no-results').hidden, true);
search.value = 'does not exist';
search.dispatchEvent(new window.Event('input'));
assert.equal(document.querySelector('#no-results').hidden, false);
const menu = document.querySelector('.menu-toggle');
menu.click();
assert.equal(menu.getAttribute('aria-expanded'), 'true');
document.dispatchEvent(new window.KeyboardEvent('keydown', { key: 'Escape' }));
assert.equal(menu.getAttribute('aria-expanded'), 'false');
assert.match(document.querySelector('#migration').textContent, /not a replacement/i);
window.close();
console.log('Docs navigation smoke test passed');
