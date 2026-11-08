import { spawn } from 'node:child_process';
import { mkdir, readdir, copyFile, access, writeFile } from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import manifest, { style } from './manifest.mjs';

const root = path.resolve('build/readme-assets');
await mkdir(path.join(root, 'raw'), { recursive: true });
await mkdir(path.join(root, 'logs'), { recursive: true });
const concurrency = Math.max(1, Math.min(4, Number(process.env.IMAGE_CONCURRENCY) || 2));
let cursor = 0;
let failed = false;
async function generate(item) {
  const destination = path.join(root, 'raw', `${item.id}.png`);
  try { await access(destination); console.log(`SKIP ${item.id}`); return; } catch {}
  console.log(`START ${item.id}`);
  const prompt = `Do not run shell commands. Do not read, write, or copy files using shell or file tools. Only call the built-in image generation tool (image_gen), generate exactly one image, then reply with its saved path. Image: ${item.prompt}\nShape: ${item.shape}. Transparent background: ${item.transparent}.\n${style}`;
  const child = spawn('codex', ['exec', '--skip-git-repo-check', '--sandbox', 'read-only', '-c', 'model_reasoning_effort="low"', '--color', 'never', '-'], { windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'] });
  let log = '';
  for (const stream of [child.stdout, child.stderr]) stream.on('data', chunk => { log += chunk.toString(); });
  child.stdin.end(prompt);
  const timeout = setTimeout(() => child.kill(), 600000);
  const code = await new Promise((resolve, reject) => { child.on('error', reject); child.on('close', resolve); });
  clearTimeout(timeout);
  await writeFile(path.join(root, 'logs', `${item.id}.log`), log);
  const session = log.match(/session id:\s*([\da-f-]+)/i)?.[1];
  if (code !== 0 || !session) throw new Error(`Codex exit ${code}; session ${session}; see log`);
  const folder = path.join(os.homedir(), '.codex', 'generated_images', session);
  const files = (await readdir(folder)).filter(name => name.endsWith('.png'));
  if (files.length !== 1) throw new Error(`Expected one PNG in ${folder}, found ${files.length}`);
  await copyFile(path.join(folder, files[0]), destination);
  console.log(`OK ${item.id}: ${destination}`);
}
await Promise.all(Array.from({ length: concurrency }, async () => {
  while (cursor < manifest.length) {
    const item = manifest[cursor++];
    try { await generate(item); } catch (error) { failed = true; console.error(`FAIL ${item.id}: ${error.message}`); }
  }
}));
process.exitCode = failed ? 1 : 0;
