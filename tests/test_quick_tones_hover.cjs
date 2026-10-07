const {chromium}=require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
const path=require('node:path');
(async()=>{
 const b=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH || undefined});
 try {
 const p=await b.newPage();const errors=[];p.on('pageerror',e=>errors.push(e.message));
 await p.addInitScript(()=>{localStorage.setItem('fy-guide-seen','yes');window.listeners={};window.__JUCE__={backend:{addEventListener:(n,f)=>(listeners[n]??=[]).push(f),emitEvent:()=>{}}}});
 await p.goto(pathToFileURL(path.resolve('prototype/index.html')).href);
 for(const width of [1920,1366,1280]){
  await p.setViewportSize({width,height:800});
  await p.mouse.move(width-30,780);await p.waitForTimeout(400);
  const box=await p.locator('.nav-item[data-page=sounds]').boundingBox();
  await p.evaluate(()=>{window.changes=[];window.observer?.disconnect();window.observer=new MutationObserver(()=>changes.push(document.querySelector('#quick-tones').classList.contains('open')));observer.observe(document.querySelector('#quick-tones'),{attributes:true,attributeFilter:['class']})});
  await p.mouse.move(box.x+box.width/2,box.y+box.height/2);await p.waitForTimeout(2300);
  const open=()=>p.locator('#quick-tones').evaluate(e=>e.classList.contains('open'));
  assert(await open(),'stationary pointer closed');
  for(let i=0;i<10;i++){await p.mouse.move(box.x+10+i*10,box.y+15+i%3);await p.waitForTimeout(80);}
  assert(await open(),'movement within button closed');
  assert(!(await p.evaluate(()=>changes)).includes(false),'menu flickered');
  await p.mouse.move(box.x+box.width+30,box.y+30,{steps:10});await p.waitForTimeout(500);
  assert(await open(),'crossing to menu closed');
  await p.keyboard.press('Escape');await p.waitForTimeout(300);assert(!(await open()));
  await p.mouse.move(box.x+30,box.y+20);await p.waitForTimeout(450);assert(await open());
  await p.mouse.move(width-20,780);await p.waitForTimeout(550);
  assert(!(await open()),'outside failed to close');
  console.log('PASS hover, movement, crossing, escape, reentry, leave',width);
 }
 assert.deepEqual(errors,[]);
 } finally {await b.close();}
})().catch(e=>{console.error(e);process.exit(1)});
