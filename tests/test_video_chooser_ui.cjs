const {chromium}=require('playwright'),assert=require('node:assert/strict'),path=require('node:path'),{pathToFileURL}=require('node:url');
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH});
 try {
  const page=await browser.newPage({viewport:{width:1366,height:768}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>{
   localStorage.setItem('fy-guide-seen','yes');localStorage.setItem('fy-voice-enabled','no');
   window.listeners={};window.sent=[];
   window.__JUCE__={backend:{addEventListener:(name,fn)=>{const old=listeners[name];listeners[name]=s=>{old?.(s);fn(s)}},emitEvent:(n,p)=>sent.push({n,p})}};
  });
  await page.goto(pathToFileURL(path.resolve('prototype/index.html')).href);
  await page.evaluate(()=>listeners.backendState({activated:true,featuresUnlocked:true,pluginLoaded:true,deviceConnected:true,deviceMatched:true}));
  await page.locator('#choose-video').click();
  assert.equal(await page.evaluate(()=>sent.filter(e=>e.n==='chooseVideo').length),1);
  assert(!await page.locator('body').innerText().then(t=>t.includes('请选择一个伴奏视频')),'no misleading pre-selection toast');
  assert.equal(await page.evaluate(()=>originalVideoSource),'');
  await page.evaluate(()=>listeners.videoSelected({engine:'vlc',generation:1,url:'file:///C:/中文路径/伴奏一.mp4',name:'伴奏一.mp4'}));
  await page.evaluate(()=>listeners.nativeVideoState({engine:'vlc',generation:1,ready:true,duration:30,position:0,playing:false,viewportEpoch:0}));
  assert.equal(await page.locator('#video-name').innerText(),'伴奏一.mp4');
  await page.locator('#play-button').click();
  assert(await page.evaluate(()=>sent.some(e=>e.n==='setVideoPlaybackState'&&e.p.playing)));
  await page.evaluate(()=>sent=[]);
  await page.locator('#change-video').click();
  assert.equal(await page.evaluate(()=>sent.filter(e=>e.n==='chooseVideo').length),1);
  // Cancel has no videoSelected callback. Keep the old selection and its
  // generation; opening the picker only pauses existing playback as before.
  assert.equal(await page.evaluate(()=>originalVideoSource),'file:///C:/中文路径/伴奏一.mp4');
  assert.equal(await page.evaluate(()=>videoGeneration),1);
  assert(!await page.evaluate(()=>sent.some(e=>['clearVideo','loadVideo','seekVideo'].includes(e.n))));
  await page.locator('#play-button').click();
  assert(await page.evaluate(()=>sent.some(e=>e.n==='setVideoPlaybackState'&&e.p.generation===1&&e.p.playing)));
  await page.locator('#fullscreen').click();
  await page.locator('#change-video').click();
  await page.evaluate(()=>listeners.videoSelected({engine:'vlc',generation:2,url:'file:///C:/中文路径/伴奏二.mp4',name:'伴奏二.mp4'}));
  assert.equal(await page.evaluate(()=>videoGeneration),2);
  assert.equal(await page.locator('#video-name').innerText(),'伴奏二.mp4');
  assert.equal(await page.evaluate(()=>videoWantsPlaying),false,'replacement must not autoplay');
  assert.deepEqual(errors,[]);
  console.log('PASS actual page add/change buttons, native event, cancel preserves selection, replay, expanded replacement, Chinese path and no autoplay');
 }finally{await browser.close()}
})().catch(e=>{console.error(e);process.exit(1)});
