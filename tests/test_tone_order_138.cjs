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
 const oldOrder=['builtin:kong-pipa','builtin:trumpet-2','builtin:kong-clarinet-2'];
 await page.evaluate(o=>localStorage.setItem('fengyin-card-order-swam',JSON.stringify(o)),oldOrder);
 await page.reload();await show();assert.deepEqual(await ids(),expected.map(id=>'builtin:'+id));
 assert.equal(await page.locator('.card-drag-handle,[draggable=true],#restore-tone-order').count(),0);
 console.log('PASS: exact 41-tone default; fixed publisher order after reload; legacy drag order ignored; no sort handles.');
}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
