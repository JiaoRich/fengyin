const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
const path=require('node:path');
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH||undefined});
 try{
 const p=await browser.newPage({viewport:{width:1366,height:768}});const errors=[];p.on('pageerror',e=>errors.push(e.message));
 await p.addInitScript(()=>{localStorage.setItem('fy-guide-seen','yes');localStorage.setItem('fy-voice-enabled','yes');window.events=[];window.listeners={};window.deliver=(n,v)=>(listeners[n]||[]).forEach(f=>f(v));window.__JUCE__={backend:{addEventListener:(n,f)=>(listeners[n]??=[]).push(f),emitEvent:(n,v)=>events.push([n,v])}};});
 await p.goto(pathToFileURL(path.resolve('prototype/index.html')).href);
 await p.evaluate(()=>fengyinVoice.guide.start());
 for(let i=0;i<25;i++){
  assert.equal(await p.evaluate(()=>fengyinVoice.guide.step),i);
  const box=await p.locator('#fy-guide').boundingBox();assert(box && box.x>=0 && box.y>=0 && box.y+box.height<=770);
  await p.locator('#fy-guide [data-guide=next]').click();
 }
 assert.equal(await p.evaluate(()=>fengyinVoice.guide.active),false);
 await p.evaluate(()=>{window.countdownResult=null;fengyinVoice.beforeRecording().then(v=>countdownResult=v);});
 assert.equal(await p.evaluate(()=>countdownResult),null);
 await p.evaluate(()=>{const clip=events.filter(e=>e[0]==='playVoice').at(-1);if(clip[1].id!=='V041')throw Error('wrong countdown');deliver('voiceFinished',clip[1].token);});
 assert.equal(await p.evaluate(()=>countdownResult),true);
 await p.evaluate(()=>{fengyinVoice.say('V001');deliver('backendState',{breath:.5,deviceConnected:true,pluginLoaded:true});});
 assert.equal(await p.evaluate(()=>events.at(-1)[0]),'stopVoice');
 await p.evaluate(()=>{deliver('backendState',{breath:0,recording:true});window.refused=null;fengyinVoice.say('V001').then(v=>refused=v);});
 assert.equal(await p.evaluate(()=>refused),false);
 assert.deepEqual(errors,[]);console.log('PASS 25 guide steps, viewport bounds, countdown completion, speech cancellation and recording guard');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
