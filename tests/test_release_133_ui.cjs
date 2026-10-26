const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
const path=require('node:path');
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH});
 try {
  const page=await browser.newPage({viewport:{width:1366,height:900}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>{
   localStorage.setItem('fy-guide-seen','yes');localStorage.setItem('fy-voice-enabled','no');
   window.sent=[];window.callbacks={};window.deliver=(n,p)=>(callbacks[n]||[]).forEach(f=>f(p));
   window.__JUCE__={backend:{addEventListener:(n,f)=>(callbacks[n]??=[]).push(f),emitEvent:(name,payload)=>sent.push({name,payload})}};
  });
  await page.goto(pathToFileURL(path.resolve('prototype/index.html')).href);
  await page.evaluate(()=>{
   window.releaseState={activated:true,deviceConnected:true,deviceMatched:true,pluginLoaded:true,
    breath:0,noteReceived:false,instrumentKey:'kong-pipa',pluginName:'QinEngineV3',
    techniqueMappings:[{id:'mute',technique:8,mode:2,sourceType:4,sourceNumber:24,pluginSupported:true}]};
   deliver('backendState',releaseState);document.querySelector('#license-lock').hidden=true;showPage('settings');
  });
  assert.equal(await page.locator('[data-page="audio"]').count(),0);
  for(const selector of ['#audio-rate-select','#audio-buffer-select','#fy-guide-open','#studio-entry'])
   assert(await page.locator(selector).isVisible(),selector);
  assert.equal(await page.locator('#audio-auto-optimize,#low-performance').count(),0);
  await page.locator('#studio-entry').click();
  const buttons=await page.locator('#studio-login-cancel,#studio-login-confirm').evaluateAll(es=>es.map(e=>{
   const s=getComputedStyle(e);return {height:e.getBoundingClientRect().height,radius:s.borderRadius};
  }));
  assert.equal(buttons.length,2);assert.deepEqual(buttons[0],buttons[1]);
  await page.locator('#studio-login-cancel').click();
  await page.locator('#fy-guide-open').click();
  const titles=[];
  for(let n=0;n<40&&await page.evaluate(()=>fengyinVoice.guide.active);++n){
   titles.push(await page.locator('#fy-guide h3').innerText());
   if(await page.locator('#fy-guide [data-guide="next"]').isDisabled()){
    await page.evaluate(()=>deliver('backendState',{...releaseState,breath:.3,noteReceived:true,note:67}));
   }
   assert.equal(await page.locator('#fy-guide [data-guide="next"]').isDisabled(),false);
   await page.locator('#fy-guide [data-guide="next"]').click();
  }
  assert.equal(await page.evaluate(()=>fengyinVoice.guide.active),false);
  for(const title of ['录音与录屏','我的录制','精简模式','授权与激活','调音师工作台','随时重新查看'])
   assert(titles.includes(title),title);
  assert(titles.length>=20);
  await page.evaluate(()=>{deliver('backendState',{...releaseState,breath:0});showPage('wind');});
  assert.equal(await page.locator('[data-technique-id="mute"] h3').innerText(),'技巧音色');
  await page.evaluate(()=>showPage('play'));
  const sizes=await page.locator('#record,#recording-manager').evaluateAll(es=>es.map(e=>{
   const s=getComputedStyle(e);return {height:e.getBoundingClientRect().height,font:s.fontSize};
  }));assert.deepEqual(sizes[0],sizes[1]);
  await page.locator('#record').click();assert(await page.locator('#record-menu').isVisible());
  await page.locator('[data-record-kind="video"]').click();
  assert(await page.evaluate(()=>sent.some(x=>x.name==='toggleScreenRecording')));
  assert.match(await page.locator('#record').innerText(),/准备/);
  const previousToast=await page.locator('#toast').innerText();
  await page.evaluate(()=>deliver('recordingResult',{success:true,message:'录制成功！在这里查看录屏文件'}));
  assert(await page.locator('#record-success').isVisible());
  assert.match(await page.locator('#record-success span').innerText(),/录制成功/);
  assert.equal(await page.locator('#toast').innerText(),previousToast,'success must not create a duplicate toast');
  assert.deepEqual(errors,[]);
  console.log(`PASS ${titles.length} guide steps, advanced settings, matched dialog buttons, generic CH2 UI and one recording-success banner`);
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
