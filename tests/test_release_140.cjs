const {chromium}=require('playwright'),fs=require('fs'),path=require('path'),assert=require('node:assert/strict'),{pathToFileURL}=require('node:url');
const tones=[...fs.readFileSync('native/Source/PublishedToneCatalog.h','utf8').matchAll(/\{ "([^"]+)", "([^"]+)", "([^"]+)", "([^"]+)", PublishedToneKind::(\w+),/g)].map(m=>({id:m[1],name:m[2],instrumentKey:m[3],styleId:m[4],kind:m[5].startsWith('kong')?'kong':m[5].startsWith('kontakt')?'kontakt':'package'}));
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
 try {
  const page=await browser.newPage({viewport:{width:1366,height:768}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>{localStorage.setItem('fy-guide-seen','yes');window.listeners={};window.sent=[];window.__JUCE__={backend:{addEventListener:(n,f)=>{const old=window.listeners[n];window.listeners[n]=s=>{old?.(s);f(s)}},emitEvent:(n,p)=>window.sent.push({n,p})}}});
  await page.goto(pathToFileURL(path.resolve('prototype/index.html')).href);
  const state={activated:true,featuresUnlocked:true,deviceConnected:true,deviceMatched:true,pluginLoaded:true,pluginLoading:false,breath:0,publishedTones:tones,presets:[],instruments:[]};
  const expected=tones.map(t=>'builtin:'+t.id);
  const show=async()=>{await page.evaluate(s=>window.listeners.backendState(s),state);await page.locator('[data-page="sounds"]').click()};
  const ids=()=>page.locator('#preset-grid>section').evaluateAll(ns=>ns.map(n=>n.dataset.cardKey));
  await show();assert.deepEqual(await ids(),expected);
  for(const size of [[1366,768],[1024,600],[1920,1080]]){
   await page.setViewportSize({width:size[0],height:size[1]});
   const geometry=await page.locator('#preset-grid h3').evaluateAll(ns=>ns.map(n=>{const r=n.getBoundingClientRect(),p=n.parentElement.getBoundingClientRect();return {text:n.textContent,delta:Math.abs((r.left+r.right-p.left-p.right)/2),font:getComputedStyle(n).fontSize}}));
   assert(geometry.every(x=>x.delta<1&&x.font==='20px'),JSON.stringify(geometry));
  }
  await page.evaluate(()=>localStorage.setItem('fengyin-card-order-swam',JSON.stringify(['builtin:kong-pipa','builtin:trumpet'])));
  await page.reload();await show();assert.deepEqual(await ids(),expected,'legacy drag order must not override publisher order');
  assert.equal(await page.locator('#restore-tone-order,.card-drag-handle,[draggable="true"]').count(),0);
  const custom=[{id:'studio-a',name:'自建方案甲',published:true},{id:'studio-b',name:'自建方案乙',published:true},{id:'draft',name:'草稿',published:true,studioDraft:true}];
  await page.evaluate(s=>window.listeners.backendState(s),{...state,presets:custom,publishedTones:[...tones].reverse()});
  assert.deepEqual(await ids(),[...expected,'custom:studio-a','custom:studio-b'],'all builtins keep fixed order; user tones follow; drafts stay hidden');
  await page.reload();await show();assert.deepEqual(await ids(),expected);
  await page.locator('[data-page="play"]').click();
  for(const tone of tones){
   await page.evaluate(s=>window.listeners.backendState(s),{...state,instrumentKey:tone.instrumentKey,instrumentChineseName:'shared plugin label',activeToneVariantId:'builtin:'+tone.id});
   await page.locator('#instrument-picture').evaluate(async n=>{await n.decode()});
   const image=await page.locator('#instrument-picture').evaluate(n=>({hidden:n.hidden,width:n.naturalWidth,alt:n.alt,source:n.getAttribute('src')}));
   const media=await page.evaluate(id=>window.FENGYIN_TONE_MEDIA[id],tone.id);
   assert(image.width>0&&!image.hidden&&image.alt.includes(tone.name)&&image.source.endsWith(media.artwork),JSON.stringify({tone,image}));
  }
  // Independent switches: tone announcements still work with general guidance off.
  await page.locator('[data-page="settings"]').click();
  await page.locator('#fy-voice-enabled').uncheck();await page.locator('#fy-voice-tones').check();
  await page.locator('[data-page="play"]').click();
  await page.evaluate(s=>window.listeners.backendState({...s,instrumentKey:'alto-sax',instrumentChineseName:'中音萨克斯'}),state);
  for(const tone of tones){
   const played=await page.evaluate(name=>{window.sent=[];window.fengyinVoice.tone(name);const event=window.sent.find(e=>e.n==='playVoice');if(event)window.listeners.voiceFinished(event.p.token);return event?.p.id},tone.name);
   const expected=await page.evaluate(id=>window.FENGYIN_TONE_MEDIA[id].voiceId,tone.id);assert.equal(played,expected,tone.name+' exact announcement');
  }
  // OFF is a selectable mode, not an unavailable instrument. Validate both
  // repaired-role buttons and global hardware binding retention after rerender.
  const mappings=[{id:'vibrato',technique:1,mode:4,pluginSupported:true,sourceType:2,sourceNumber:0},{id:'growl',technique:0,mode:4,pluginSupported:true,sourceType:1,sourceNumber:20}];
  await page.evaluate(({s,m})=>window.listeners.backendState({...s,instrumentKey:'alto-sax',instrumentChineseName:'中音萨克斯',techniqueMappings:m}),{s:state,m:mappings});
  await page.locator('[data-page="wind"]').click();
  for(const role of ['vibrato','growl'])for(const mode of ['breath','hardware','off']){
   const button=page.locator(`#technique-list [data-technique-id="${role}"] [data-technique-mode="${mode}"]`);assert(await button.isEnabled());await button.click();
   const event=await page.evaluate(()=>window.sent.filter(e=>e.n==='setTechniqueConfiguration').at(-1));assert.equal(event.p.techniqueId,role);assert.equal(event.p.mode,mode);
  }
  await page.locator('[data-page="play"]').click();
  for(const mode of [0,1,2]){await page.locator(`#music-tempo [data-tempo="${mode}"]`).click();const e=await page.evaluate(()=>window.sent.filter(e=>e.n==='setMusicTempo').at(-1));assert.equal(e.p.mode,mode)}
  await page.setViewportSize({width:1366,height:768});
  const bench=await page.evaluate(()=>{
   currentPluginLoaded=true;lastRingLevel=-1;const start=performance.now();
   for(let i=0;i<10000;i++)updateBreathRing(i%101);
   const msPerUpdate=(performance.now()-start)/10000;
   updateBreathRing(0);const initial=document.querySelector('#breath-ring').outerHTML;
   for(let i=0;i<1000;i++)updateBreathRing(0);
   return {msPerUpdate,idleUnchanged:document.querySelector('#breath-ring').outerHTML===initial,paths:document.querySelectorAll('#breath-ring path').length};
  });
  assert(bench.msPerUpdate<.1,JSON.stringify(bench));assert(bench.idleUnchanged&&bench.paths===3);
  for(const compact of [false,true]){
   await page.setViewportSize(compact?{width:480,height:560}:{width:1366,height:768});
   await page.evaluate(({s,compact})=>{window.listeners.backendState({...s,compactMode:compact,breath:.9,instrumentKey:'kong-jinghu',instrumentChineseName:'京胡',activeToneVariantId:'builtin:kong-jinghu'});updateBreathRing(90)},{s:state,compact});
   const ring=await page.locator('#breath-ring').evaluate(n=>({display:getComputedStyle(n).display,opacity:+getComputedStyle(n).opacity,rect:n.getBoundingClientRect().toJSON(),box:n.getBBox()}));
   assert(ring.display!=='none'&&ring.opacity>0&&ring.rect.height>0);assert.equal(await page.locator('#breath-wind').count(),0);
   await page.screenshot({path:`outputs/release-140-20261030/ring-${compact?'minimum':'full'}.png`});
  }
  assert.deepEqual(errors,[]);
  fs.writeFileSync('outputs/release-140-20261030/ui-results.json',JSON.stringify({tones:41,geometricCenter:true,fixedPublisherOrder:true,userTonesAtEnd:true,allArtworkDecoded:true,exactSpeechIds:true,globalTechniqueButtons:true,fullCompactBreathRings:true,bench,limits:'Local browser only; not Windows GPU or live audio test'},null,2));
  console.log('PASS fixed 41-tone order + user tones appended + 41 geometric centers/artwork/speech identities + global technique controls + full/compact breath ring:',bench);
 }finally{await browser.close()}
})().catch(e=>{console.error(e);process.exit(1)});
