import assert from 'node:assert/strict';
import { synthesize, prompts, settings } from '../scripts/generate_recording_prompts.mjs';

const auth = { APPID: 'test', APIKey: 'test', APISecret: 'test' };
let request;
class FakeSocket {
  constructor(url) { assert.equal(new URL(url).hostname, 'cbm01.cn-huabei-1.xf-yun.com'); queueMicrotask(() => this.onopen()); }
  send(body) {
    request = JSON.parse(body);
    // Exercise sequence sorting, not network arrival order.
    this.onmessage({ data: JSON.stringify({ header: { code: 0, status: 1 }, payload: { audio: { seq: 1, audio: Buffer.from([3, 4]).toString('base64') } } }) });
    this.onmessage({ data: JSON.stringify({ header: { code: 0, status: 2 }, payload: { audio: { seq: 0, audio: Buffer.from([1, 2]).toString('base64') } } }) });
  }
  close() { this.onclose?.(); }
}
const result = await synthesize(auth, prompts[0][1], FakeSocket);
assert.equal(result.toString('ascii', 0, 4), 'RIFF');
assert.equal(result.readUInt32LE(24), 24000);
assert.deepEqual([...result.subarray(44)], [1, 2, 3, 4]);
assert.equal(Buffer.from(request.payload.text.text, 'base64').toString(), prompts[0][1]);
assert.equal(request.parameter.tts.vcn, 'x6_lingyuyan_pro');
assert.deepEqual(request.parameter.tts, settings);
await assert.rejects(synthesize({}, 'test', FakeSocket), /Missing XFYUN credentials/);
class FailureSocket extends FakeSocket {
  send() { this.onmessage({ data: JSON.stringify({ header: { code: 11200, message: 'private details must not leak' } }) }); }
}
await assert.rejects(synthesize(auth, 'test', FailureSocket), error => error.message === 'XFYUN error code 11200');
console.log('PASS recording prompt protocol, PCM, ordering, voice, redacted errors; no network requests');
