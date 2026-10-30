// XFYUN official protocol: https://www.xfyun.cn/doc/spark/super%20smart-tts.html
// Credentials stay in memory. Never store keys or signed URLs in the repository.
import { createHmac, createHash } from 'node:crypto';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

export const prompts = [
  ['REC_PREP_AUDIO', '准备录音，将在滴声后开始。'],
  ['REC_PREP_SCREEN', '准备录屏，将在滴声后开始。'],
  ['REC_SAVED', '录制已保存，可以在我的录制中查看。'],
];
export const settings = { vcn: 'x6_lingyuyan_pro', speed: 50, volume: 50, pitch: 50,
  bgs: 0, reg: 0, rdn: 0, rhy: 0,
  audio: { encoding: 'raw', sample_rate: 24000, channels: 1, bit_depth: 16, frame_size: 0 } };

function wav(pcm) {
  if (!pcm.length || pcm.length % 2) throw new Error('Invalid PCM response');
  const header = Buffer.alloc(44);
  header.write('RIFF'); header.writeUInt32LE(36 + pcm.length, 4); header.write('WAVEfmt ', 8);
  header.writeUInt32LE(16, 16); header.writeUInt16LE(1, 20); header.writeUInt16LE(1, 22);
  header.writeUInt32LE(24000, 24); header.writeUInt32LE(48000, 28);
  header.writeUInt16LE(2, 32); header.writeUInt16LE(16, 34);
  header.write('data', 36); header.writeUInt32LE(pcm.length, 40);
  return Buffer.concat([header, pcm]);
}

export async function synthesize(auth, text, WebSocketClient = globalThis.WebSocket) {
  for (const key of ['APPID', 'APIKey', 'APISecret'])
    if (!auth[key] || auth[key].includes('*')) throw new Error('Missing XFYUN credentials');
  const host = 'cbm01.cn-huabei-1.xf-yun.com', path = '/v1/private/mcd9m97e6';
  const date = new Date().toUTCString();
  const signature = createHmac('sha256', auth.APISecret)
    .update(`host: ${host}\ndate: ${date}\nGET ${path} HTTP/1.1`).digest('base64');
  const authorization = Buffer.from(`api_key="${auth.APIKey}", algorithm="hmac-sha256", headers="host date request-line", signature="${signature}"`).toString('base64');
  return new Promise((resolve, reject) => {
    const socket = new WebSocketClient(`wss://${host}${path}?${new URLSearchParams({ host, date, authorization })}`);
    const frames = new Map(); let finished = false;
    const finish = (error, result) => {
      if (finished) return; finished = true; clearTimeout(timer); socket.close();
      error ? reject(error) : resolve(result);
    };
    const timer = setTimeout(() => finish(new Error('XFYUN synthesis timeout')), 60000);
    socket.onopen = () => socket.send(JSON.stringify({
      header: { app_id: auth.APPID, status: 2 }, parameter: { tts: settings },
      payload: { text: { encoding: 'utf8', compress: 'raw', format: 'plain', status: 2,
        seq: 0, text: Buffer.from(text).toString('base64') } }
    }));
    socket.onerror = event => {
      const code = event.error?.cause?.code || event.error?.code || 'HANDSHAKE';
      const message = String(event.error?.message || '');
      const reason = ['Received network error or non-101 status code.', 'Request was cancelled.', 'Connection was closed before it was established.'].find(x => message.includes(x)) || 'No public detail';
      finish(new Error(`XFYUN connection failed (${String(code).replace(/[^A-Z0-9_]/g, '')}): ${reason}`));
    };
    socket.onclose = () => { if (!finished) finish(new Error('XFYUN closed before final audio')); };
    socket.onmessage = event => {
      try {
        const response = JSON.parse(event.data);
        if (response.header?.code !== 0) return finish(new Error(`XFYUN error code ${response.header?.code}`));
        const audio = response.payload?.audio;
        if (audio?.audio) frames.set(audio.seq, Buffer.from(audio.audio, 'base64'));
        if (response.header?.status === 2 || audio?.status === 2)
          finish(null, wav(Buffer.concat([...frames].sort((a, b) => a[0] - b[0]).map(x => x[1]))));
      } catch { finish(new Error('Invalid XFYUN audio response')); }
    };
  });
}

// Output is a staging directory. Review the WAVs before replacing shipped assets.
export async function generate(auth, directory, WebSocketClient = globalThis.WebSocket, requestedPrompts = prompts) {
  await mkdir(directory, { recursive: true });
  const audit = [];
  for (const [id, text] of requestedPrompts) {
    const requestHash = createHash('sha256').update(JSON.stringify({ text, settings })).digest('hex');
    const file = join(directory, `${id}.wav`), receipt = join(directory, `${id}.json`);
    let bytes;
    try {
      const previous = JSON.parse(await readFile(receipt, 'utf8'));
      const existing = await readFile(file);
      if (previous.requestHash === requestHash && previous.sha256 === createHash('sha256').update(existing).digest('hex')) bytes = existing;
    } catch { /* No verified cached result. */ }
    if (!bytes) bytes = await synthesize(auth, text, WebSocketClient);
    const entry = { id, text, voice: settings.vcn, settings, requestHash,
      sha256: createHash('sha256').update(bytes).digest('hex'), seconds: (bytes.length - 44) / 48000 };
    await writeFile(file, bytes); await writeFile(receipt, JSON.stringify(entry, null, 2) + '\n');
    audit.push(entry);
  }
  await writeFile(join(directory, 'manifest.json'), JSON.stringify(audit, null, 2) + '\n');
  return audit;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  // Private stdin, not argv, shell history, log files, or persistent configuration.
  let input = '';
  for await (const chunk of process.stdin) {
    input += chunk;
    if (input.includes('\n')) { input = input.split('\n')[0]; break; }
  }
  try {
    let auth;
    try { auth = JSON.parse(input); } catch { throw new Error('Invalid credential input (not logged)'); }
    input = '';
    const client = process.argv[3] ? (await import(pathToFileURL(process.argv[3]).href)).default : globalThis.WebSocket;
    const result = await generate(auth, process.argv[2], client);
    console.log(JSON.stringify(result.map(({ id, voice, seconds, sha256 }) => ({ id, voice, seconds, sha256 }))));
  } catch (error) { console.error(error.message); process.exitCode = 1; }
}
