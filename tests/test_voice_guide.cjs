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
 await p.evaluate(()=>fengyinVoice.guide.start('basic'));
 for(let i=0;i<5;i++){
  assert.equal(await p.evaluate(()=>fengyinVoice.guide.step),i);
  const box=await p.locator('#fy-guide').boundingBox();assert(box && box.x>=0 && box.y>=0 && box.y+box.height<=770);
  if(i===0){assert(await p.locator('[data-guide=next]').isDisabled());await p.evaluate(()=>deliver('backendState',{deviceConnected:true,deviceMatched:true,pluginLoaded:true,breath:0}));}
  if(i===4){assert(await p.locator('[data-guide=next]').isDisabled());await p.evaluate(()=>deliver('backendState',{deviceConnected:true,deviceMatched:true,pluginLoaded:true,breath:.2,noteReceived:true}));}
  await p.locator('#fy-guide [data-guide=next]').click();
 }
 assert.equal(await p.evaluate(()=>fengyinVoice.guide.active),false);
 await p.evaluate(()=>{deliver('backendState',{deviceConnected:true,deviceMatched:true,pluginLoaded:true,breath:0});fengyinVoice.guide.start('advanced');});
 for(let i=0;i<9;i++){
  const spot=await p.locator('#fy-guide-spot').boundingBox();assert(spot&&spot.width>0&&spot.height>0,'advanced target must be visible '+i);
  await p.locator('#fy-guide [data-guide=next]').click();
 }
 assert.deepEqual(await p.evaluate(()=>fengyinVoice.recordingPreferences()),{announce:true,volume:.45});
 // Preparation is owned by the native recorder now. Completion still announces
 // once, after the previous busy state is cleared, for both recording kinds.
 for(const kind of ['audio','screen']){
  await p.evaluate(()=>{deliver('backendState',{recording:true,screenRecordingBusy:true});events=[];});
  await p.evaluate(kind=>deliver('recordingResult',{success:true,kind}),kind);
  assert.equal(await p.evaluate(()=>events.filter(e=>e[0]==='playVoice'&&e[1].id==='REC_SAVED').length),1);
  await p.evaluate(()=>{const clip=events.filter(e=>e[0]==='playVoice').at(-1);deliver('voiceFinished',clip[1].token);});
 }
 await p.evaluate(()=>{fengyinVoice.say('V001');deliver('backendState',{breath:.5,deviceConnected:true,pluginLoaded:true});});
 assert.equal(await p.evaluate(()=>events.at(-1)[0]),'stopVoice');
 await p.evaluate(()=>{deliver('backendState',{breath:0,recording:true});window.refused=null;fengyinVoice.say('V001').then(v=>refused=v);});
 assert.equal(await p.evaluate(()=>refused),false);
 assert.deepEqual(errors,[]);console.log('PASS 5 guarded first-use steps, 9 visible advanced targets, native recording preferences and audio/screen completion prompts, speech cancellation and recording guard');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
