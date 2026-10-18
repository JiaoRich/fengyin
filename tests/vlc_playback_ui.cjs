const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
const path=require('node:path');
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
 try {
 const page=await browser.newPage({viewport:{width:1366,height:900}}),errors=[];
 page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{
  localStorage.setItem('fy-guide-seen','yes');window.listeners={};window.sent=[];window.callbacks={};window.htmlPlayCalls=0;
  HTMLMediaElement.prototype.play=function(){htmlPlayCalls++;return Promise.resolve();};
  window.__JUCE__={backend:{addEventListener:(name,fn)=>{
    (callbacks[name]??=[]).push(fn);listeners[name]=p=>callbacks[name].forEach(f=>f(p));
  },emitEvent:(name,payload)=>sent.push({name,payload})}};
 });
 await page.goto(pathToFileURL(path.resolve(__dirname,'../prototype/index.html')).href);
 await page.evaluate(()=>{document.querySelector('#license-lock').hidden=true;showPage('play');
   listeners.videoSelected({engine:'vlc',generation:1,url:'file:///C:/测试 video.mp4',name:'测试 video'});});
 assert.equal(await page.locator('#video').getAttribute('src'),null);
 await page.evaluate(()=>listeners.nativeVideoState({generation:1,seekRevision:0,ready:true,playing:false,position:0,duration:120}));
 await page.waitForTimeout(100);
 assert.equal(await page.locator('#duration').innerText(),'02:00');
 assert(await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload.visible));
 for(const width of [1920,1366,1050]){
  await page.setViewportSize({width,height:900});await page.waitForTimeout(100);
  const g=await page.evaluate(()=>{
   const box=s=>{const r=document.querySelector(s).getBoundingClientRect();return {x:r.x,y:r.y,w:r.width,h:r.height};};
   return {surface:box('#native-video-surface'),top:box('.video-topline'),controls:box('.video-controls')};
  });
  assert(g.surface.h>50,JSON.stringify(g));
  assert(g.surface.y>=g.top.y+g.top.h-1,'video covers title');
  assert(g.surface.y+g.surface.h<=g.controls.y+1,'video covers controls');
 }
 await page.locator('#play-button').click();
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='setVideoPlaybackState').at(-1).payload.playing),true);
 await page.evaluate(()=>listeners.nativeVideoState({generation:1,seekRevision:0,ready:true,playing:true,position:3,duration:120}));
 await page.evaluate(()=>{const s=document.querySelector('#seek');s.dispatchEvent(new Event('pointerdown'));s.value=50;s.dispatchEvent(new Event('input'));s.dispatchEvent(new Event('change'));s.dispatchEvent(new Event('pointerup'));});
 assert.deepEqual(await page.evaluate(()=>sent.filter(x=>x.name==='seekVideo').map(x=>x.payload)),[{generation:1,position:60,seekRevision:1}]);
 await page.evaluate(()=>listeners.nativeVideoState({generation:1,seekRevision:0,ready:true,playing:true,position:4,duration:120}));
 assert.equal(await page.locator('#current-time').innerText(),'01:00');
 await page.evaluate(()=>listeners.nativeVideoState({generation:1,seekRevision:1,ready:true,playing:true,position:61,duration:120}));
 assert.equal(await page.locator('#current-time').innerText(),'01:01');
 await page.evaluate(()=>{showPage('sounds');});await page.waitForTimeout(100);
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload.visible),false);
 await page.evaluate(()=>showPage('play'));await page.waitForTimeout(100);
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload.visible),true);
 await page.evaluate(()=>document.body.classList.add('quick-tones-open'));await page.waitForTimeout(100);
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload.visible),false);
 await page.evaluate(()=>document.body.classList.remove('quick-tones-open'));await page.waitForTimeout(100);
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload.visible),true);
 await page.evaluate(()=>{const d=document.createElement('section');d.id='test-modal';d.setAttribute('role','dialog');d.textContent='Test';document.body.append(d);});
 await page.waitForTimeout(100);
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload.visible),false);
 await page.evaluate(()=>document.querySelector('#test-modal').hidden=true);await page.waitForTimeout(100);
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload.visible),true);
 await page.evaluate(()=>{document.body.classList.add('presentation-hidden');updateVideoPresentation();});await page.waitForTimeout(100);
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload.visible),false);
 await page.evaluate(()=>{document.body.classList.remove('presentation-hidden');updateVideoPresentation();
   listeners.nativeVideoState({generation:1,seekRevision:1,ready:true,playing:false,finished:true,position:120,duration:120});});
 await page.locator('#play-button').click();
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='seekVideo').at(-1).payload.position),0);
 await page.evaluate(()=>{listeners.videoSelected({engine:'vlc',generation:2,url:'file:///C:/second.mp4',name:'第二个'});
   listeners.nativeVideoState({generation:1,seekRevision:99,ready:true,playing:true,position:50,duration:600});});
 assert.equal(await page.locator('#duration').innerText(),'00:00');
 await page.evaluate(()=>listeners.nativeVideoState({generation:2,seekRevision:0,ready:false,error:'无法播放',position:0,duration:0}));
 assert.equal(await page.evaluate(()=>videoLoading),false);
 assert.equal(await page.evaluate(()=>htmlPlayCalls),0);
 await page.evaluate(()=>chooseVideoFile());
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='setVideoPlaybackState').at(-1).payload.playing),false);
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='syncVideoPlayback').length),0);
 assert.equal(await page.locator('#video').getAttribute('src'),null);
 // A native player can outlive the document. A missed one-shot selection
 // must be repaired by its authoritative state without issuing play/seek.
 const oldSession=await page.evaluate(()=>videoWebSession);
 await page.reload();
 await page.evaluate(()=>{document.querySelector('#license-lock').hidden=true;showPage('sounds');});
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload.visible),false);
 assert.notEqual(await page.evaluate(()=>videoWebSession),oldSession);
 await page.evaluate(()=>listeners.nativeVideoState({engine:'vlc',url:'file:///C:/surviving.mp4',name:'仍在播放',generation:8,seekRevision:4,ready:true,playing:true,position:33,duration:120}));
 assert.equal(await page.evaluate(()=>nativeVideoEngine),true);
 assert.equal(await page.locator('#current-time').innerText(),'00:33');
 assert.equal(await page.locator('#video-name').innerText(),'仍在播放');
 assert.equal(await page.evaluate(()=>sent.filter(x=>['seekVideo','setVideoPlaybackState'].includes(x.name)).length),0);
 await page.waitForTimeout(100);
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload.visible),false);
 await page.evaluate(()=>showPage('play'));await page.waitForTimeout(100);
 const restoredViewport=await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload);
 assert.equal(restoredViewport.visible,true);assert.equal(restoredViewport.generation,8);
 assert.equal(restoredViewport.session,await page.evaluate(()=>videoWebSession));
 await page.evaluate(()=>listeners.videoSelected({engine:'vlc',url:'file:///C:/surviving.mp4',name:'仍在播放',generation:8}));
 assert.equal(await page.locator('#current-time').innerText(),'00:33','duplicate selection reset transport');
 await page.evaluate(()=>listeners.nativeVideoState({engine:'vlc',url:'file:///C:/old.mp4',name:'旧状态',generation:7,seekRevision:99,ready:true,playing:true,position:50,duration:600}));
 assert.equal(await page.locator('#video-name').innerText(),'仍在播放');
 assert.deepEqual(errors,[]);
 console.log('PASS VLC UI no HTML decoder/audio/clock, viewport avoids controls and hides for pages/dialogs, single explicit seeks, stale rejection, EOF replay and errors');
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
