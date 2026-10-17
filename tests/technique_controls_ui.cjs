// NODE_PATH=<runtime node_modules> node tests/technique_controls_ui.cjs
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
  localStorage.setItem('fy-guide-seen','yes');window.listeners={};window.sent=[];window.eventCallbacks={};
  window.__JUCE__={backend:{addEventListener:(name,fn)=>{
   (eventCallbacks[name]??=[]).push(fn);listeners[name]=payload=>eventCallbacks[name].forEach(callback=>callback(payload));
  },emitEvent:(name,payload)=>sent.push({name,payload})}};
 });
 await page.goto(pathToFileURL(path.resolve(__dirname,'../prototype/index.html')).href);
 await page.evaluate(()=>{
  document.querySelector('#license-lock').hidden=true;
  currentPluginLoaded=true;currentInstrument={instrumentKey:'violin'};
  latestBackendState={activated:true,pluginLoaded:true,deviceConnected:true,deviceMatched:true,deviceName:'test wind',instrumentKey:'violin'};
  techniqueMappings=[{id:'vibrato',technique:1,mode:1,sourceType:1,sourceNumber:74,strength:.55},
   {id:'growl',technique:0,mode:2,sourceType:1,sourceNumber:75,strength:.5},
   {id:'mute',technique:8,mode:2,sourceType:4,sourceNumber:24,strength:.5}];
  showPage('wind');renderSmartAdapter(latestBackendState);
 });
 const cards=page.locator('#technique-list .technique-card');
 assert.deepEqual(await cards.locator('h3').allTextContents(),['颤音','嘶吼音','其他技巧']);
 const labels={vibrato:['关闭','自动','咬嘴控制'],growl:['关闭','自动','按键控制'],mute:['关闭','按键控制']};
 for(const [id,expected] of Object.entries(labels)){
  const card=page.locator(`[data-technique-id="${id}"]`);
  assert.deepEqual(await card.locator('[data-technique-mode]').allTextContents(),expected);
 }
 assert.equal(await page.locator('#adapter-summary').isVisible(),false);
 assert.equal(await page.locator('#adapter-guide-detail').isVisible(),false);
 assert.equal(await page.locator('#adapter-inline-hint').isVisible(),false);
 assert.equal(await cards.locator('p,.recommendation').count(),0);
 const vib=page.locator('[data-technique-id="vibrato"]');
 assert.equal(await vib.locator('.technique-map-actions').isVisible(),false);
 for(const id of ['vibrato','growl','mute']){
  const card=page.locator(`[data-technique-id="${id}"]`);
  await card.locator('[data-technique-mode="hardware"]').click();
  assert.equal(await card.locator('.technique-map-actions').isVisible(),true);
  assert.equal(await card.locator('.learn-source').innerText(),'重新映射');
  assert.equal(await card.locator('.clear-source').innerText(),'清除映射');
  const a=await card.locator('.learn-source').boundingBox(),b=await card.locator('.clear-source').boundingBox();
  assert(Math.abs(a.width-b.width)<1&&a.height===b.height);
  assert(a.y>=(await card.locator('[data-technique-mode="hardware"]').boundingBox()).y+44);
  await card.locator('[data-technique-mode="off"]').click();
  assert.equal(await card.locator('.technique-map-actions').isVisible(),false);
  if(id!=='mute'){
   await card.locator('[data-technique-mode="breath"]').click();
   assert.equal(await card.locator('.technique-map-actions').isVisible(),false);
  }
 }
 // Choosing manual control reveals the secondary action; it must not start
 // learning or open a dialog until the user clicks that action explicitly.
 await page.evaluate(()=>{techniqueMappings.find(m=>m.id==='vibrato').sourceType=0;techniqueMappings.find(m=>m.id==='vibrato').sourceNumber=-1;techniqueRenderSignature='';renderAdapterTechniqueGrid();sent.length=0;});
 await vib.locator('[data-technique-mode="hardware"]').click();
 assert.equal(await vib.locator('.learn-source').innerText(),'点击映射');
 assert.equal(await vib.locator('.clear-source').isVisible(),false);
 assert.equal(await page.locator('#technique-learning-dialog').isVisible(),false);
 assert.equal(await page.evaluate(()=>sent.some(e=>e.name==='beginTechniqueLearn')),false);
 await vib.locator('.learn-source').click();
 assert.equal(await page.locator('#technique-learning-dialog').isVisible(),true);
 assert.equal(await page.evaluate(()=>sent.filter(e=>e.name==='beginTechniqueLearn').length),1);
 await page.evaluate(()=>{
  const m=techniqueMappings.find(m=>m.id==='vibrato');Object.assign(m,{mode:2,sourceType:1,sourceNumber:74});
  listeners.techniqueLearnResult({success:true,message:'映射成功'});
 });
 assert.equal(await vib.locator('.learn-source').innerText(),'重新映射');
 await vib.locator('.clear-source').click();
 assert.equal(await page.evaluate(()=>sent.filter(e=>e.name==='removeTechniqueMapping').at(-1).payload.technique),1);
 assert.equal(await vib.locator('.technique-map-actions').isVisible(),false);
 assert.equal(await page.evaluate(()=>techniqueMappings.find(m=>m.id==='growl').sourceNumber),75);
 assert.equal(await page.evaluate(()=>techniqueMappings.find(m=>m.id==='mute').sourceNumber),24);
 await vib.locator('[data-technique-mode="hardware"]').click();
 assert.equal(await vib.locator('.learn-source').innerText(),'点击映射');
 // Retain device match instructions only while a deliberate match is active.
 await page.evaluate(()=>setAdapterMatchPhase('matching',latestBackendState));
 assert.equal(await page.locator('#adapter-guide-detail').isVisible(),true);
 await page.evaluate(()=>setAdapterMatchPhase('ready',latestBackendState));
 assert.equal(await page.locator('#adapter-guide-detail').isVisible(),false);
 for(const width of [1920,1366,1050,960,680]){
  await page.setViewportSize({width,height:1000});
  const geometry=await cards.evaluateAll(nodes=>nodes.map(e=>{
   const modes=Array.from(e.querySelectorAll('[data-technique-mode]'));
   return {y:e.getBoundingClientRect().y,overflow:e.scrollWidth>e.clientWidth,
    rows:modes.map(b=>b.getBoundingClientRect().y),buttonOverflow:modes.some(b=>b.scrollWidth>b.clientWidth)};
  }));
  for(const g of geometry){assert(!g.overflow&&!g.buttonOverflow,'overflow at '+width);assert(g.rows.every(y=>y===g.rows[0]),'mode row wrapped at '+width);}
  if(width>=1366)assert(geometry.every(g=>g.y===geometry[0].y),'cards must be horizontal');
 }
 await page.setViewportSize({width:1366,height:900});
 await page.screenshot({path:'/private/tmp/fy120-technique-approved-ui.png',fullPage:true});
 assert.deepEqual(errors,[]);
 console.log('PASS approved three-column UI, mode order/labels, secondary actions, learn/clear, no unsolicited learning, compact text, five widths; no JS errors');
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
