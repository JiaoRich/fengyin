const {chromium}=require('playwright'),fs=require('fs'),path=require('path'),assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
const tones=JSON.parse(fs.readFileSync('assets/tone-packages/release-139-audit.json')).tones.map(r=>({id:r.id,name:r.name,instrumentKey:r.instrumentKey,kind:r.brand}));
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
 try {
  for(const dpr of [1,2]){
   const page=await browser.newPage({viewport:{width:1280,height:800},deviceScaleFactor:dpr}),errors=[];
   page.on('pageerror',e=>errors.push(e.message));
   await page.addInitScript(()=>{
    localStorage.setItem('fy-guide-seen','yes');localStorage.setItem('fy-voice-enabled','no');
    window.listeners={};window.sent=[];window.rectFaults=[];window.rectCount=0;
    window.__JUCE__={backend:{addEventListener:(n,f)=>{const old=window.listeners[n];window.listeners[n]=s=>{old?.(s);f(s)}},emitEvent:(n,p)=>window.sent.push({n,p})}};
    const original=CanvasRenderingContext2D.prototype.fillRect;
    CanvasRenderingContext2D.prototype.fillRect=function(x,y,w,h){if(this.canvas.id==='spectrum'&&h>0){window.rectCount++;const height=this.canvas.getBoundingClientRect().height;if(y<-.01||y+h>height+.01)window.rectFaults.push({y,h,height});}return original.call(this,x,y,w,h)};
   });
   await page.goto(pathToFileURL(path.resolve('prototype/index.html')).href);
   const state={activated:true,featuresUnlocked:true,deviceConnected:true,deviceMatched:true,breath:1,pluginLoaded:true,pluginLoading:false,instrumentKey:'trumpet',activeToneVariantId:'builtin:trumpet',publishedTones:tones,instruments:[],presets:[]};
   await page.evaluate(s=>window.listeners.backendState(s),state);
   await page.locator('[data-page="sounds"]').click();
   const cards=page.locator('#preset-grid .instrument-preset-card');assert.equal(await cards.count(),41);
   const labels=await cards.locator('h3').evaluateAll(ns=>ns.map(n=>({font:getComputedStyle(n).fontSize,align:getComputedStyle(n).textAlign})));assert(labels.every(x=>x.font==='20px'&&x.align==='center'));
   assert.equal(await page.locator('[data-card-key="builtin:kontakt-kensa-2"]').count(),0);
   const custom=['builtin:kong-pipa','builtin:kontakt-kensa-2','builtin:trumpet'];
   await page.evaluate(o=>localStorage.setItem('fengyin-card-order-swam',JSON.stringify(o)),custom);
   await page.reload();await page.evaluate(s=>window.listeners.backendState(s),state);await page.locator('[data-page="sounds"]').click();
   assert.deepEqual(await cards.evaluateAll(ns=>ns.slice(0,2).map(n=>n.dataset.cardKey)),['builtin:kong-pipa','builtin:trumpet']);
   assert.deepEqual(await page.evaluate(()=>JSON.parse(localStorage.getItem('fengyin-card-order-swam'))),custom,'removing a built-in must not rewrite user order');
   await page.locator('[data-page="play"]').click();
   for(const [width,height] of [[1280,720],[1280,800],[1366,768],[1920,1080],[1024,600],[800,600]]){
    await page.setViewportSize({width,height});await page.evaluate(s=>window.listeners.backendState(s),state);await page.waitForTimeout(150);
    const box=await page.evaluate(()=>{const c=document.querySelector('#spectrum'),r=c.getBoundingClientRect();return {top:r.top,bottom:r.bottom,height:r.height,pixels:c.height,view:innerHeight,meters:document.querySelectorAll('#meter-l,#meter-r,.meters').length};});
    assert(box.height>10&&box.top>=0&&box.bottom<=height,JSON.stringify({width,height,box}));assert.equal(box.meters,0);assert.equal(box.pixels,Math.round(box.height*dpr));
   }
   await page.evaluate(s=>window.listeners.backendState({...s,compactMode:true}),state);await page.waitForTimeout(100);
   assert(await page.locator('body').evaluate(n=>n.classList.contains('compact-mode')));
   assert.deepEqual(await page.evaluate(()=>window.rectFaults),[]);assert((await page.evaluate(()=>window.rectCount))>0);
   assert.deepEqual(errors,[]);await page.close();
  }
  console.log('PASS: 41 cards20px centered; removed built-in/user-order migration; 6 window sizes x2 DPI; all spectrum bars inside actual height; no JS errors.');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
