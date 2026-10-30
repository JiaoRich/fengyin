// Regenerate the approved active XFYUN speech catalogue. Credentials
// remain in private stdin; output contains text/audio receipts, never credentials.
import {readFile} from 'node:fs/promises';
import {pathToFileURL} from 'node:url';
import {generate, settings} from './generate_recording_prompts.mjs';
let input='';
try {
  for await (const chunk of process.stdin) {
    input+=chunk;
    if(input.includes('\n')){input=input.split('\n')[0];break;}
  }
  const auth=JSON.parse(input);input='';
  const plan=JSON.parse(await readFile(new URL('../assets/voice/release-140-audit.json',import.meta.url),'utf8'));
  if(!plan.every(x=>x.voice===settings.vcn))throw new Error('Original reviewed voice required');
  const client=process.argv[3]?(await import(pathToFileURL(process.argv[3]).href)).default:globalThis.WebSocket;
  const result=await generate(auth,process.argv[2],client,plan.map(x=>[x.id,x.text]));
  console.log(JSON.stringify(result.map(({id,voice,seconds,sha256})=>({id,voice,seconds,sha256}))));
} catch {console.error('Tone speech generation failed; credentials were not logged.');process.exitCode=1;}
