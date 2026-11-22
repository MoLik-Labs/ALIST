#!/usr/bin/env node
import { readFile, writeFile } from 'node:fs/promises';
import { gzipSync } from 'node:zlib';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const checkOnly = process.argv.includes('--check');
const paths = {
  html: path.join(root, 'web/index.html'),
  css: path.join(root, 'web/styles.css'),
  js: path.join(root, 'web/app.js'),
  output: path.join(root, 'include/alist/web_ui.hpp')
};

function fail(message) {
  console.error(`Web UI build failed: ${message}`);
  process.exit(1);
}

function minifyCss(source) {
  let output = '';
  let quote = '';
  for (let i = 0; i < source.length; i++) {
    const ch = source[i];
    if (quote) {
      output += ch;
      if (ch === '\\') output += source[++i] || '';
      else if (ch === quote) quote = '';
      continue;
    }
    if (ch === '"' || ch === "'") { quote = ch; output += ch; continue; }
    if (ch === '/' && source[i + 1] === '*') {
      i += 2;
      while (i < source.length && !(source[i] === '*' && source[i + 1] === '/')) i++;
      i++;
      continue;
    }
    if (/\s/.test(ch)) {
      while (i + 1 < source.length && /\s/.test(source[i + 1])) i++;
      const prev = output.at(-1) || '';
      const next = source[i + 1] || '';
      if (prev && next && !/[{}:;,>~]/.test(prev) && !/[{}:;,>~]/.test(next)) output += ' ';
      continue;
    }
    output += ch;
  }
  return output.trim();
}

function minifyJs(source) {
  let index = 0;
  const isWord = ch => Boolean(ch) && /[\p{L}\p{N}_$]/u.test(ch);
  const needsSpace = (left, right) => {
    if (isWord(left) && isWord(right)) return true;
    if ((left === '+' && right === '+') || (left === '-' && right === '-') ||
        (left === '/' && (right === '/' || right === '*')) ||
        (left === '=' && ['=', '>', '+'].includes(right)) || (left === '!' && right === '=') ||
        (left === '<' && ['=', '<'].includes(right)) || (left === '>' && ['=', '>'].includes(right)) ||
        (left === '&' && right === '&') || (left === '|' && right === '|') ||
        (left === '?' && ['?', '.'].includes(right)) || (left === '*' && right === '*')) return true;
    return false;
  };
  function scanQuoted(quote) {
    let out = quote;
    index++;
    while (index < source.length) {
      const ch = source[index++]; out += ch;
      if (ch === '\\') { if (index < source.length) out += source[index++]; continue; }
      if (ch === quote) break;
    }
    return out;
  }
  function scanTemplate() {
    let out = '`'; index++;
    while (index < source.length) {
      const ch = source[index++]; out += ch;
      if (ch === '\\') { if (index < source.length) out += source[index++]; continue; }
      if (ch === '`') break;
      if (ch === '$' && source[index] === '{') {
        out += '{'; index++;
        out += scanCode(true);
      }
    }
    return out;
  }
  function scanCode(stopAtBrace = false) {
    let out = '';
    let pendingSpace = false;
    let pendingNewline = false;
    let braceDepth = 0;
    while (index < source.length) {
      const ch = source[index];
      if (stopAtBrace && ch === '}' && braceDepth === 0) { index++; return `${out}}`; }
      if (ch === '"' || ch === "'") {
        const token = scanQuoted(ch);
        const prev = out.at(-1) || '';
        if (pendingNewline) out = out.replace(/[ \t]+$/g, '') + '\n';
        else if (pendingSpace && needsSpace(prev, token[0])) out += ' ';
        pendingSpace = false; pendingNewline = false; out += token; continue;
      }
      if (ch === '`') {
        const token = scanTemplate();
        const prev = out.at(-1) || '';
        if (pendingNewline) out = out.replace(/[ \t]+$/g, '') + '\n';
        else if (pendingSpace && needsSpace(prev, token[0])) out += ' ';
        pendingSpace = false; pendingNewline = false; out += token; continue;
      }
      if (ch === '/' && source[index + 1] === '/' && source[index - 1] !== '\\') {
        index += 2; while (index < source.length && source[index] !== '\n') index++;
        pendingNewline = true; pendingSpace = false; continue;
      }
      if (ch === '/' && source[index + 1] === '*' && source[index - 1] !== '\\') {
        index += 2; let hadNewline = false;
        while (index < source.length && !(source[index] === '*' && source[index + 1] === '/')) {
          if (source[index] === '\n') hadNewline = true;
          index++;
        }
        index = Math.min(source.length, index + 2);
        if (hadNewline) pendingNewline = true; else pendingSpace = true;
        continue;
      }
      if (/\s/.test(ch)) {
        if (ch === '\n' || ch === '\r') pendingNewline = true;
        else pendingSpace = true;
        index++;
        while (index < source.length && /[ \t\f\v]/.test(source[index])) index++;
        continue;
      }
      const prev = out.at(-1) || '';
      if (pendingNewline) out = out.replace(/[ \t]+$/g, '') + '\n';
      else if (pendingSpace && needsSpace(prev, ch)) out += ' ';
      pendingSpace = false; pendingNewline = false;
      if (stopAtBrace) {
        if (ch === '{') braceDepth++;
        else if (ch === '}' && braceDepth > 0) braceDepth--;
      }
      out += ch; index++;
    }
    if (stopAtBrace) fail('unterminated template expression');
    return out.trim();
  }
  return scanCode(false).replace(/[ \t]+\n/g, '\n').replace(/\n{2,}/g, '\n').trim();
}

function parseI18n(js) {
  const startToken = 'const I18N = ';
  const start = js.indexOf(startToken);
  const end = js.indexOf('\n\n  const TEXT_IDS', start);
  if (start < 0 || end < 0) fail('could not locate the bilingual dictionary in web/app.js');
  const literal = js.slice(start + startToken.length, end).replace(/;\s*$/, '');
  try {
    const context = Object.create(null);
    vm.runInNewContext(`globalThis.dictionary = (${literal})`, context, { timeout: 1000 });
    return context.dictionary;
  } catch (error) { fail(`invalid bilingual dictionary: ${error.message}`); }
}

function checkViolet(css) {
  if (/\b(violet|purple|lavender|lilac)\b/i.test(css)) fail('violet/purple color names are not allowed in the design system');
  const hexes = css.match(/#[0-9a-f]{3,8}\b/gi) || [];
  for (let hex of hexes) {
    hex = hex.slice(1);
    if (hex.length === 3 || hex.length === 4) hex = [...hex.slice(0, 3)].map(char => char + char).join('');
    if (hex.length !== 6 && hex.length !== 8) continue;
    const [r, g, b] = [0, 2, 4].map(index => parseInt(hex.slice(index, index + 2), 16) / 255);
    const max = Math.max(r, g, b), min = Math.min(r, g, b), delta = max - min;
    let hue = 0;
    if (delta) {
      if (max === r) hue = 60 * (((g - b) / delta) % 6);
      else if (max === g) hue = 60 * ((b - r) / delta + 2);
      else hue = 60 * ((r - g) / delta + 4);
      if (hue < 0) hue += 360;
    }
    if (hue >= 270 && hue <= 330 && delta > 0.12) fail(`violet hue ${hex} (${hue.toFixed(1)}°) is not allowed`);
  }
}

const [htmlRaw, cssRaw, jsRaw] = await Promise.all([
  readFile(paths.html, 'utf8'), readFile(paths.css, 'utf8'), readFile(paths.js, 'utf8')
]);
const cssTag = '<link rel="stylesheet" href="/styles.css">';
const jsTag = '<script src="/app.js" defer></script>';
if (htmlRaw.split(cssTag).length !== 2 || htmlRaw.split(jsTag).length !== 2) fail('index.html must contain exactly one local CSS and JS asset tag');

const dictionary = parseI18n(jsRaw);
const enKeys = Object.keys(dictionary.en || {}).sort();
const viKeys = Object.keys(dictionary.vi || {}).sort();
if (JSON.stringify(enKeys) !== JSON.stringify(viKeys)) {
  fail(`EN/VI translation keys differ: missing EN [${viKeys.filter(key => !enKeys.includes(key)).join(', ')}], missing VI [${enKeys.filter(key => !viKeys.includes(key)).join(', ')}]`);
}
checkViolet(cssRaw.replace(/\/\*[\s\S]*?\*\//g, ''));

const jsSyntax = spawnSync(process.execPath, ['--check', '--input-type=commonjs', '-'], { input: jsRaw, encoding: 'utf8' });
if (jsSyntax.status !== 0) fail(`JavaScript syntax error:\n${jsSyntax.stderr || jsSyntax.stdout}`);
const js = minifyJs(jsRaw);
const minSyntax = spawnSync(process.execPath, ['--check', '--input-type=commonjs', '-'], { input: js, encoding: 'utf8' });
if (minSyntax.status !== 0) fail(`minified JavaScript syntax error:\n${minSyntax.stderr || minSyntax.stdout}`);
const css = minifyCss(cssRaw);
const htmlShell = htmlRaw.replace(/<!--[\s\S]*?-->/g, '').replace(/[ \t]+$/gm, '').replace(/^\s+$/gm, '').trim();
const html = htmlShell.replace(cssTag, `<style>\n${css}\n</style>`).replace(jsTag, `<script>\n${js}\n</script>`);
const scriptOpen = (html.match(/<script>/gi) || []).length;
const scriptClose = (html.match(/<\/script>/gi) || []).length;
if (scriptOpen !== 1 || scriptClose !== 1) fail(`embedded UI must have exactly one inline script (found ${scriptOpen} open, ${scriptClose} close)`);
const markupWithoutScript = html.replace(/<script>[\s\S]*?<\/script>/i, '<script></script>');
if (/\son[a-z][a-z0-9_-]*\s*=/i.test(markupWithoutScript)) fail('inline on* event handlers are forbidden; use delegated data-act listeners');
if (html.includes(' onclick=')) fail('inline onclick attribute text is forbidden by strict CSP');
if (/<(?:script|link|img)[^>]+(?:src|href)=["']https?:\/\//i.test(html)) fail('external CDN or remote asset found; the Workspace App must be self-contained');

const scriptDigest = createHash('sha256').update(js).digest('base64');
let delimiter = 'KOVA_WEB_UI';
while (html.includes(`)${delimiter}"`)) delimiter += '_X';
const header = `#pragma once\n\n#include <string_view>\n\nnamespace alist {\n\n// Generated by scripts/build_webui.mjs from web/index.html, web/styles.css, and web/app.js.\ninline constexpr std::string_view kEmbeddedWebUiHtml = R"${delimiter}(${html})${delimiter}";\n\n} // namespace alist\n`;
const rawBytes = Buffer.byteLength(html);
const gzipBytes = gzipSync(Buffer.from(html), { level: 9 }).length;
const jsBytes = Buffer.byteLength(js);
const oldHeader = await readFile(paths.output, 'utf8').catch(() => '');
if (checkOnly) {
  if (oldHeader !== header) fail('include/alist/web_ui.hpp is stale; run node scripts/build_webui.mjs');
} else {
  await writeFile(paths.output, header);
}
console.log(`Web UI ${checkOnly ? 'verified' : 'built'}: ${rawBytes.toLocaleString()} B raw, ${gzipBytes.toLocaleString()} B gzip, ${jsBytes.toLocaleString()} B JS, SHA-256 ${scriptDigest}`);
console.log(`Translations: ${enKeys.length} EN/VI keys; inline script: ${scriptOpen} block; violet palette: none.`);
