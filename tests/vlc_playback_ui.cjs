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
 await page.evaluate(()=>listeners.nativeVideoViewportReset({session:videoWebSession,epoch:5}));
 await page.waitForTimeout(100);
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload.epoch),5);
 const revisionBeforeHide=await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload.revision);
 await page.evaluate(()=>{showPage('sounds');});
 const immediateHide=await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload);
 assert.equal(immediateHide.visible,false);assert(immediateHide.revision>revisionBeforeHide);
 await page.evaluate(()=>{listeners.nativeVideoViewportReset({session:'old-document',epoch:99});showPage('play');});
 await page.waitForTimeout(100);
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='nativeVideoViewport').at(-1).payload.epoch),5);
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
 // Native VLC must never enter WebView2 DOM fullscreen. Repeated in-window
 // expansion keeps HTML controls outside the reported HWND rectangle.
 await page.evaluate(()=>{window.domFullscreenCalls=0;document.querySelector('.video-card').requestFullscreen=()=>{domFullscreenCalls++;return Promise.resolve();};});
 for(let i=0;i<20;i++) {
  await page.locator('#fullscreen').click();await page.waitForTimeout(25);
  assert(await page.evaluate(()=>document.body.classList.contains('video-expanded')));
  const geometry=await page.evaluate(()=>{
   const r=document.querySelector('#native-video-surface').getBoundingClientRect();
   const t=document.querySelector('.video-topline').getBoundingClientRect();
   const c=document.querySelector('.video-controls').getBoundingClientRect();
   return {videoTop:r.top,videoBottom:r.bottom,titleBottom:t.bottom,controlsTop:c.top};
  });
  assert(geometry.videoTop>=geometry.titleBottom-1,JSON.stringify(geometry));
  assert(geometry.videoBottom<=geometry.controlsTop+1,JSON.stringify(geometry));
  if(i%2)await page.keyboard.press('Escape');else await page.locator('#fullscreen').click();
  assert.equal(await page.evaluate(()=>document.body.classList.contains('video-expanded')),false);
 }
 assert.equal(await page.evaluate(()=>domFullscreenCalls),0);
 await page.locator('#fullscreen').click();
 await page.evaluate(()=>showPage('sounds'));
 assert.equal(await page.evaluate(()=>document.body.classList.contains('video-expanded')),false);
 await page.evaluate(()=>showPage('play'));await page.waitForTimeout(50);
 // Fine volume: clicking the track does not jump; a short real drag changes
 // only a small amount; keyboard precision and final bridge value agree.
 const volume=page.locator('#video-volume');
 const b=await volume.boundingBox();
 const before=Number(await volume.inputValue());
 await page.mouse.move(b.x+5,b.y+b.height/2);await page.mouse.down();
 assert.equal(Number(await volume.inputValue()),before);
 await page.mouse.move(b.x+15,b.y+b.height/2,{steps:5});await page.mouse.up();
 const after=Number(await volume.inputValue());
 assert(after>before && after-before<3,`volume drag ${before} -> ${after}`);
 await volume.press('ArrowLeft');await page.waitForTimeout(50);
 assert(Math.abs(Number(await volume.inputValue())-(after-.1))<.011);
 assert.equal(await page.locator('#record-video-choice').count(),0);
 assert.equal(await page.locator('#record-options').count(),0);
 assert.equal(await page.locator('#record-kind').count(),0);
 await page.locator('#record').click();
 await page.locator('[data-record-kind="video"]').click();
 assert.equal(await page.evaluate(()=>sent.at(-1).name),'toggleScreenRecording');
 await page.evaluate(()=>listeners.backendState({activated:true,pluginLoaded:false,presets:[],publishedTones:[],availableInstruments:[],screenRecordingSupported:true,
   screenRecordingBusy:true,recordingPreparing:true}));
 assert.equal(await page.locator('#record').innerText(),'准备中 · 取消');
 await page.locator('#record').click();
 assert.equal(await page.evaluate(()=>sent.at(-1).name),'toggleScreenRecording');
 await page.evaluate(()=>listeners.backendState({activated:true,pluginLoaded:false,presets:[],publishedTones:[],availableInstruments:[],
   screenRecordingBusy:true,recordingSaving:true}));
 assert.equal(await page.locator('#record').innerText(),'正在保存，请稍等');
 assert.equal(await page.locator('#record').isDisabled(),true);
 await page.evaluate(()=>{listeners.backendState({activated:true,pluginLoaded:false,presets:[],publishedTones:[],availableInstruments:[]});listeners.recordingResult({success:true,message:'录制成功！在这里查看录屏文件'});});
 assert.equal(await page.locator('#record-success').isVisible(),true);
 // Both recording modes announce a successful save exactly once, even while
 // accompaniment is playing. Failure/cancellation and disabled speech stay quiet.
 await page.evaluate(()=>{document.querySelector('#fy-voice-enabled').checked=true;document.querySelector('#fy-voice-enabled').dispatchEvent(new Event('change'));sent.length=0;
   listeners.backendState({activated:true,screenRecordingBusy:true,recordingSaving:true});
   listeners.recordingResult({success:true,kind:'video',videoQualityWarning:true,message:'录制成功！在这里查看录屏文件'});});
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='playVoice'&&x.payload.id==='REC_SAVED').length),1);
 assert(!(await page.locator('#record-success').innerText()).includes('部分画面'));
 assert((await page.locator('#record-success').innerText()).includes('录制成功'));
 await page.evaluate(()=>{listeners.voiceFinished(sent.find(x=>x.name==='playVoice').payload.token);sent.length=0;
   listeners.recordingResult({success:false,message:'录制失败'});});
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='playVoice').length),0);
 await page.evaluate(()=>{listeners.recordingResult({success:true,kind:'audio',message:'录制成功！在这里查看录音文件'});});
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='playVoice'&&x.payload.id==='REC_SAVED').length),1);
 await page.evaluate(()=>{document.querySelector('#fy-voice-enabled').checked=false;document.querySelector('#fy-voice-enabled').dispatchEvent(new Event('change'));sent.length=0;listeners.recordingResult({success:true,kind:'video'});});
 assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='playVoice').length),0);
 await page.evaluate(()=>showPage('settings'));
 assert.equal(await page.locator('[data-page="audio"]').count(),0);
 assert.equal(await page.locator('#page-title').innerText(),'高级设置');
 assert.equal(await page.locator('#audio-driver-select').isVisible(),true);
 assert.equal(await page.locator('#license-code').isVisible(),false);
 assert.equal(await page.locator('#audio-auto-optimize').count(),0);
 assert.equal(await page.locator('#audio-driver-help,#audio-output-help,#audio-rate-help,#audio-buffer-help').count(),0);
 assert.equal(await page.locator('#low-performance').count(),0);
 assert.equal(await page.locator('.advanced-card:has(#fy-voice-enabled)').count(),0);
 assert.equal(await page.locator('.advanced-card:has(#studio-entry)').count(),0);
 await page.setViewportSize({width:1366,height:768});
 await page.screenshot({path:'/private/tmp/fy128-advanced-settings.png'});
 assert((await page.locator('#page-settings').boundingBox()).y+(await page.locator('#page-settings').boundingBox()).height<768,'advanced settings should fit target height');
 await page.evaluate(()=>listeners.backendState({activated:false,trialActive:true,trialRemainingSeconds:900,featuresUnlocked:true,pluginLoaded:false,presets:[],publishedTones:[],availableInstruments:[]}));
 assert.equal(await page.locator('#license-code').isVisible(),true);
 assert.equal(await page.locator('#activate').isVisible(),true);
 await page.evaluate(()=>{document.querySelector('#record-success').hidden=true;applyWindowMode(true);});
 await page.screenshot({path:'/private/tmp/fy128-compact-recording.png'});
 assert.equal(await page.locator('.compact-action-row').evaluate(el=>getComputedStyle(el).display),'grid');
 assert.equal(await page.locator('.compact-action-row').evaluate(el=>getComputedStyle(el).gridTemplateColumns.split(' ').length),2);
 for(const kind of ['audio','video']){
   await page.evaluate(()=>listeners.backendState({activated:true,featuresUnlocked:true,pluginLoaded:false,presets:[],publishedTones:[],availableInstruments:[],compactMode:true}));
   await page.locator('#record').click();
   assert.equal(await page.locator('#record-menu').isVisible(),true);
   const menu=await page.locator('#record-menu').boundingBox();assert(menu.y>=0 && menu.y+menu.height<=900);
   await page.locator(`[data-record-kind="${kind}"]`).click();
   assert.equal(await page.evaluate(()=>sent.at(-1).name),kind==='audio'?'toggleRecording':'toggleScreenRecording');
   assert.equal(await page.locator('#record-kind').count(),0);
   await page.evaluate(kind=>listeners.backendState({activated:true,featuresUnlocked:true,pluginLoaded:false,presets:[],publishedTones:[],availableInstruments:[],compactMode:true,recording:kind==='audio',screenRecordingBusy:kind==='video',recordingPreparing:true}),kind);
   assert.equal(await page.locator('#record').innerText(),'准备中 · 取消');
   await page.locator('#record').click();
   assert.equal(await page.evaluate(()=>sent.at(-1).name),kind==='audio'?'toggleRecording':'toggleScreenRecording');
 }
 await page.evaluate(()=>{listeners.backendState({activated:true,featuresUnlocked:true,pluginLoaded:false,presets:[],publishedTones:[],availableInstruments:[],compactMode:true});listeners.recordingResult({success:false,canRecover:true,message:'保存失败，素材已保留'});});
 assert.equal(await page.locator('#record-recover').isVisible(),true);
 await page.locator('#record-recover').click();assert.equal(await page.evaluate(()=>sent.at(-1).name),'resumeScreenRecording');
 await page.evaluate(()=>{applyWindowMode(false);showPage('play');});
 const recordRect=await page.locator('#record').boundingBox(),managerRect=await page.locator('#recording-manager').boundingBox(),volumeRect=await page.locator('#master-volume').boundingBox();
 assert.equal(recordRect.height,managerRect.height);
 assert.equal(recordRect.width,managerRect.width);
 assert(Math.abs(recordRect.y+recordRect.height/2-volumeRect.y-volumeRect.height/2)<3,'recording and volume must remain one row');
 await page.screenshot({path:'/private/tmp/fy128-normal-toolbar.png'});
 await page.evaluate(()=>{videoWantsPlaying=false;listeners.nativeVideoState({generation:videoGeneration,ready:true,playing:false,position:0,duration:120});showPage('settings');});
 assert(await page.locator('#fy-guide-open').isVisible());
 const guideButton=await page.locator('#fy-guide-open').boundingBox(),studioButton=await page.locator('#studio-entry').boundingBox();
 assert.equal(guideButton.height,studioButton.height);
 await page.locator('#fy-guide-open').click();
 assert(await page.locator('#fy-guide').isVisible());
 const guideTitles=[];
 for(let i=0;i<40&&await page.locator('#fy-guide').isVisible();i++){
   guideTitles.push(await page.locator('#fy-guide h3').innerText());
   assert((await page.locator('#fy-guide p').innerText()).trim().length>0);
   await page.locator('#fy-guide [data-guide=skip]').click();
 }
 assert.equal(await page.locator('#fy-guide').isVisible(),false);
 for(const title of ['录音与录屏','我的录制','声音设置','授权与激活','语音与音色播报','调音师工作台','混响','吹奏音量','伴奏音量','精简模式'])assert(guideTitles.includes(title),title);
 assert.equal(await page.evaluate(()=>sent.some(x=>x.name==='toggleWindowRecording')),false);
 assert.deepEqual(errors,[]);
 console.log('PASS VLC UI no HTML decoder/audio/clock, viewport avoids controls and hides for pages/dialogs, single explicit seeks, stale rejection, EOF replay and errors');
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
