const {chromium}=require('playwright');
const fs=require('fs'),assert=require('node:assert/strict'),{pathToFileURL}=require('node:url'),path=require('node:path');
const expected=["kontakt-kensa", "soprano-kenny", "soprano-pop", "alto-jazz", "alto-deep", "alto-pop", "alto-silky", "tenor-mellow", "tenor-warm", "tenor-air-pocket", "tenor-silky", "trumpet", "trumpet-2", "piccolo-trumpet", "alto-trombone", "bass-trombone", "flute", "piccolo", "clarinet", "kong-clarinet-2", "oboe", "kong-oboe-2", "violin-soft", "kong-violin-2", "cello", "kong-erhu", "kong-erhu-new-2", "kong-jinghu", "kong-hulusi-straight", "kong-hulusi-vibrato", "kong-qudi-straight", "kong-qudi", "kong-bangdi", "kong-nanxiao", "kong-dongxiao", "kong-xun", "kong-suona-vibrato", "kong-suona", "kong-liuqin", "kong-sanxian", "kong-pipa"];
const text=fs.readFileSync('native/Source/PublishedToneCatalog.h','utf8');
const tones=[...text.matchAll(/\{ "([^"]+)", "([^"]+)", "([^"]+)", "([^"]+)", PublishedToneKind::(\w+),/g)].map(m=>({id:m[1],name:m[2],instrumentKey:m[3],styleId:m[4],kind:m[5].startsWith('kong')?'kong':m[5].startsWith('kontakt')?'kontakt':'swam'}));
assert.deepEqual(tones.map(t=>t.id),expected);
(async()=>{
const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
try{
 const page=await browser.newPage({viewport:{width:1600,height:1000}});
 await page.addInitScript(()=>{window.listeners={};window.sent=[];window.__JUCE__={backend:{addEventListener:(n,f)=>{const previous=window.listeners[n];window.listeners[n]=s=>{previous?.(s);f(s)}},emitEvent:(n,p)=>window.sent.push({n,p})}}});
 const url=pathToFileURL(path.resolve('prototype/index.html')).href;
 await page.goto(url);
 const state={activated:true,deviceConnected:false,publishedTones:tones,instruments:[],presets:[]};
 async function show(){await page.evaluate(s=>window.listeners.backendState(s),state);await page.locator('[data-page="sounds"]').click();}
 async function ids(){return page.locator('.instrument-preset-card').evaluateAll(ns=>ns.map(n=>n.dataset.cardKey));}
 await show();
 assert.deepEqual(await ids(),expected.map(id=>'builtin:'+id));
 assert.equal(await page.evaluate(()=>localStorage.getItem('fengyin-card-order-swam')),null,'default must not write user order');
 await page.locator('.instrument-preset-card .card-drag-handle').nth(0).dragTo(page.locator('.instrument-preset-card .card-drag-handle').nth(3));
 const dragged=await ids();assert.notDeepEqual(dragged,expected.map(id=>'builtin:'+id));
 assert.deepEqual(await page.evaluate(()=>JSON.parse(localStorage.getItem('fengyin-card-order-swam'))),dragged);
 await page.reload();await show();assert.deepEqual(await ids(),dragged,'saved drag order survives reload');
 const custom=['builtin:kong-pipa','builtin:trumpet-2','builtin:kong-clarinet-2'];
 await page.evaluate(o=>localStorage.setItem('fengyin-card-order-swam',JSON.stringify(o)),custom);
 await page.reload();await show();const result=await ids();assert.deepEqual(result.slice(0,3),custom);
 assert.deepEqual(result.slice(3),expected.map(id=>'builtin:'+id).filter(id=>!custom.includes(id)));
 assert.deepEqual(await page.evaluate(()=>JSON.parse(localStorage.getItem('fengyin-card-order-swam'))),custom,'upgrade must not overwrite saved order');
 console.log('PASS: exact 41-tone default; real drag/drop; reload persistence; existing partial user order preserved.');
}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
