const {chromium}=require('playwright'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),{pathToFileURL}=require('node:url');
const rows=JSON.parse(fs.readFileSync('assets/tone-packages/dj-source-audit.json','utf8'));
(async()=>{
const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
try{
 const page=await browser.newPage({viewport:{width:1366,height:768}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{localStorage.setItem('fy-guide-seen','yes');window.listeners={};window.__JUCE__={backend:{addEventListener:(n,f)=>{const old=window.listeners[n];window.listeners[n]=x=>{old?.(x);f(x)}},emitEvent:()=>{}}}});
 await page.goto(pathToFileURL(path.resolve('prototype/index.html')).href);
 const tones=rows.map(r=>({id:r.id,name:r.name,instrumentKey:r.instrumentKey,styleId:'dj-source',kind:'package'}));
 const base={activated:true,featuresUnlocked:true,deviceConnected:true,deviceMatched:true,pluginLoaded:true,publishedTones:[{id:'alto-jazz',name:'爵士中萨',instrumentKey:'alto-sax',kind:'package'},...tones],presets:[{id:'copy',name:'DJ自存',published:true,pluginId:'a',djSourceChain:true}],instruments:rows.map(r=>({instrumentKey:r.instrumentKey,pluginId:'a',brand:'swam'})),activeToneVariantId:'builtin:dj-source-04',instrumentKey:'alto-sax',djSourceChain:true};
 await page.evaluate(s=>window.listeners.backendState(s),base);await page.locator('[data-page="sounds"]').click();
 assert.equal(await page.locator('.dj-source-card').count(),30);assert.equal(await page.locator('.instrument-preset-card:not(.dj-source-card)').count(),1);
 const djColour=await page.locator('.dj-source-card').first().evaluate(n=>getComputedStyle(n).borderColor);
 const normalColour=await page.locator('.instrument-preset-card:not(.dj-source-card)').evaluate(n=>getComputedStyle(n).borderColor);assert.notEqual(djColour,normalColour);
 for(const dj of [true,false,true,false]){
  await page.evaluate(s=>{window.listeners.backendState(s);renderExpertControls()}, {...base,djSourceChain:dj,activeToneVariantId:dj?'builtin:dj-source-04':'builtin:alto-jazz'});
  assert.equal(await page.locator('#performance-reverb').isDisabled(),dj);
  assert.deepEqual(await page.locator('[data-expert]').evaluateAll(ns=>ns.map(n=>n.disabled)),Array(13).fill(dj));
 }
 for(const size of [[1366,768],[1024,600],[390,760]]){
  await page.setViewportSize({width:size[0],height:size[1]});
  const bad=await page.locator('#preset-grid h3').evaluateAll(ns=>ns.filter(n=>n.scrollWidth>n.clientWidth+1&&getComputedStyle(n).textOverflow!=='ellipsis').map(n=>n.textContent));assert.deepEqual(bad,[]);
 }
 assert.deepEqual(errors,[]);console.log('DJ orange cards/custom copy; all DSP controls disabled/restored; 3 sizes passed');
}finally{await browser.close()}
})().catch(e=>{console.error(e);process.exitCode=1});
