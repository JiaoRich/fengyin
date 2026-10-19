// Browser-only regression: no native audio or target-machine acceptance claim.
const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
const path=require('node:path');
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
 try {
  const page=await browser.newPage(),errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>{
   localStorage.setItem('fy-guide-seen','yes');localStorage.setItem('fy-voice-enabled','no');
   window.callbacks={};window.sent=[];window.__JUCE__={backend:{addEventListener:(n,f)=>(callbacks[n]??=[]).push(f),emitEvent:(name,payload)=>sent.push({name,payload})}};
  });
  await page.goto(pathToFileURL(path.resolve(__dirname,'../prototype/index.html')).href);
  const result=await page.evaluate(()=>{
   const state={activated:true,featuresUnlocked:true,deviceConnected:true,deviceMatched:true,
    deviceName:'offline regression',deviceIdentifier:'render-test',pluginLoaded:true,
    instrumentKey:'cello',instrumentChineseName:'大提琴',pluginName:'SWAM Cello 3',
    musicTempoMode:1,breath:.6,instrumentPeak:.5,outputLeftPeak:.5,outputRightPeak:.5,
    latency:128/48,audioCpu:.8,instruments:[],effects:[],presets:[],publishedTones:[]};
   const emit=s=>{for(const cb of callbacks.backendState||[])cb(s);};emit(state);
   const select=document.querySelector('#tone-style-select'),chain=document.querySelector('#chain-style-select');
   const original=Array.from(select.options),originalChain=Array.from(chain.options);
   const watcher=new MutationObserver(()=>{});watcher.observe(select,{childList:true});
   const settingsWatcher=new MutationObserver(()=>{});
   settingsWatcher.observe(document.querySelector('#page-wind'),{subtree:true,attributes:true,childList:true,characterData:true});
   const started=performance.now();
   for(let i=0;i<100;++i)emit({...state,breath:.2+i%8/10,audioCpu:.3+i%7/10});
   for(let i=0;i<100;++i)for(const cb of callbacks.backendMeterState||[])cb({breath:.7,audioCpu:.75,note:65,noteReceived:true});
   const records=watcher.takeRecords(),settingsChanges=settingsWatcher.takeRecords().length;
   watcher.disconnect();settingsWatcher.disconnect();
   const protectedStyle=getComputedStyle(document.querySelector('#breath-wave')).display;
   const beforeCpu=document.querySelector('#cpu').textContent;
   const fullStateRetained=latestBackendState.pluginName==='SWAM Cello 3'&&latestBackendState.musicTempoMode===1;
   emit({...state,musicTempoMode:0});
   const tempoUpdated=document.querySelector('#music-tempo [data-tempo="0"]').classList.contains('active');
   return {elapsedMs:performance.now()-started,optionMutations:records.length,
    settingsChanges,protectedStyle,beforeCpu,tempoUpdated,fullStateRetained,
    optionsStable:original.every((n,i)=>select.options[i]===n),
    chainStable:originalChain.every((n,i)=>chain.options[i]===n),optionCount:select.options.length};
  });
  console.log(JSON.stringify(result));
  assert.deepEqual(errors,[]);
  assert(result.optionCount>0);
  assert.equal(result.optionMutations,0,'meter-only events must not replace style option nodes');
  assert.equal(result.settingsChanges,0,'meter-only events must not mutate hidden settings panels');
  assert.equal(result.protectedStyle,'none','disable decorative blurred waves during live performance');
  assert.equal(result.beforeCpu,'75%','separate meter packets must still update CPU');
  assert(result.fullStateRetained,'meter packets must not erase the plugin/configuration snapshot');
  assert(result.tempoUpdated,'configuration changes must bypass the meter-only fast path');
  assert(result.optionsStable&&result.chainStable,'preserve live controls during meter updates');
  await page.waitForTimeout(150);
  await page.evaluate(()=>{
   sent.length=0;
   for(let i=0;i<20;++i)nativeEvent('nativeVideoViewport',{visible:false});
   nativeEvent('windowAction',{action:'pointer-end'});nativeEvent('syncVideoPlayback',{});
  });
  await page.waitForTimeout(150);
  assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='requestBackendState').length),0,'layout/clock traffic must not wake full snapshots');
  await page.evaluate(()=>{nativeEvent('setMusicTempo',{mode:0});nativeEvent('setMusicTempo',{mode:1});});
  await page.waitForTimeout(150);
  assert.equal(await page.evaluate(()=>sent.filter(x=>x.name==='requestBackendState').length),1,'user changes must coalesce into a prompt configuration refresh');
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
