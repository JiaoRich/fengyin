const {chromium}=require('playwright'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),{pathToFileURL}=require('node:url');
const rows=JSON.parse(fs.readFileSync('assets/tone-packages/dj-source-audit.json','utf8'));
(async()=>{
const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
try{
 const page=await browser.newPage({viewport:{width:1366,height:768}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{localStorage.setItem('fy-guide-seen','yes');window.listeners={};window.__JUCE__={backend:{addEventListener:(n,f)=>{const old=window.listeners[n];window.listeners[n]=x=>{old?.(x);f(x)}},emitEvent:(n,p)=>{(window.emitted??=[]).push([n,p])}}}});
 await page.goto(pathToFileURL(path.resolve('prototype/index.html')).href);
 const tones=rows.map(r=>({id:r.id,name:r.name,instrumentKey:r.instrumentKey,styleId:'dj-source',kind:'package'}));
 const base={activated:true,featuresUnlocked:true,deviceConnected:true,deviceMatched:true,pluginLoaded:true,publishedTones:[{id:'alto-jazz',name:'爵士中萨',instrumentKey:'alto-sax',kind:'package'},...tones],presets:[{id:'copy',name:'DJ自存',published:true,pluginId:'a',djSourceChain:true}],instruments:rows.map(r=>({instrumentKey:r.instrumentKey,pluginId:'a',brand:'swam'})),activeToneVariantId:'builtin:dj-source-04',instrumentKey:'alto-sax',djSourceChain:true};
 await page.evaluate(s=>window.listeners.backendState(s),base);await page.locator('[data-page="sounds"]').click();
 assert.equal(await page.locator('.dj-source-card').count(),30);assert.equal(await page.locator('.instrument-preset-card:not(.dj-source-card)').count(),1);
 await page.addStyleTag({content:'*,*::before,*::after{transition:none!important}'});
 const style=selector=>page.locator(selector).evaluate(n=>{
  const s=getComputedStyle(n),h=getComputedStyle(n.querySelector('h3'));
  return [s.borderColor,s.backgroundColor,s.backgroundImage,s.boxShadow,h.color];
 });
 for(const theme of ['neon','spring','summer','autumn','winter','china-red','gold','minimal']){
  await page.evaluate(theme=>{const s=document.querySelector('#theme');s.value=theme;s.dispatchEvent(new Event('change'));},theme);
  await page.evaluate(s=>window.listeners.backendState(s),{...base,activeToneVariantId:''});
  const regular=await style('[data-card-key="builtin:alto-jazz"]');
  assert.deepEqual(await style('[data-card-key="builtin:dj-source-02"]'),regular,theme+' normal DJ');
  assert.deepEqual(await style('.dj-source-card:has(.delete-tone)'),regular,theme+' saved DJ');
  await page.evaluate(s=>window.listeners.backendState(s),{...base,activeToneVariantId:'builtin:alto-jazz'});
  const selected=await style('[data-card-key="builtin:alto-jazz"]');
  await page.evaluate(s=>window.listeners.backendState(s),{...base,activeToneVariantId:'builtin:dj-source-02'});
  assert.deepEqual(await style('[data-card-key="builtin:dj-source-02"]'),selected,theme+' active DJ');
 }
 for(const external of [false,true])for(const dj of [true,false,true,false])for(const busy of [false,true]){
  await page.evaluate(s=>{window.listeners.backendState(s);renderExpertControls()}, {...base,djSourceChain:dj,externalReverb:external,reverbAvailable:true,pluginLoading:busy,reverbMix:.2,activeToneVariantId:dj?'builtin:dj-source-04':'builtin:alto-jazz'});
  assert.equal(await page.locator('#performance-reverb').isDisabled(),external?busy:dj);
  const controls=await page.locator('[data-expert]').evaluateAll(ns=>ns.map(n=>[n.dataset.expert,n.disabled]));
  for(const [kind,disabled] of controls)assert.equal(disabled,busy || (kind==='reverb'&&external?false:dj||(external&&['room','damping','width'].includes(kind))));
  assert.equal(await page.locator('#performance-reverb').getAttribute('max'),external?'100':'60');
 }

 for(const value of [0,20,80,100]) {
  await page.evaluate(s=>window.listeners.backendState(s),{...base,externalReverb:true,reverbAvailable:true,pluginLoading:false,reverbMix:value/100});
  assert.equal(Number(await page.locator('#performance-reverb').inputValue()),value);
  await page.locator('#performance-reverb').evaluate((n,v)=>{n.value=String(v);n.dispatchEvent(new Event('input',{bubbles:true}));},value);
  const emitted=await page.evaluate(()=>window.emitted.filter(x=>x[0]==='setPerformanceReverb').at(-1));
  assert.equal(emitted[1].value,value/100);
 }
 await page.evaluate(s=>window.listeners.backendState(s),{...base,externalReverb:true,reverbAvailable:false});
 assert(await page.locator('#performance-reverb').isDisabled());
 for(const size of [[1366,768],[1024,600],[390,760]]){
  await page.setViewportSize({width:size[0],height:size[1]});
  const bad=await page.locator('#preset-grid h3').evaluateAll(ns=>ns.filter(n=>n.scrollWidth>n.clientWidth+1&&getComputedStyle(n).textOverflow!=='ellipsis').map(n=>n.textContent));assert.deepEqual(bad,[]);
 }
 assert.deepEqual(errors,[]);console.log('DJ cards/custom copy match regular themes in 8 themes and active states; legacy/new reverb, other DSP controls, busy/bypass states; 3 sizes passed');
}finally{await browser.close()}
})().catch(e=>{console.error(e);process.exitCode=1});
