const {chromium}=require('playwright'),assert=require('node:assert/strict'),path=require('node:path'),{pathToFileURL}=require('node:url');
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
 try{
 const page=await browser.newPage({viewport:{width:1366,height:768}}),errors=[];
 page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{window.listeners={};window.sent=[];window.__JUCE__={backend:{addEventListener:(n,f)=>(window.listeners[n]??=[]).push(f),emitEvent:(n,p)=>window.sent.push([n,p])}};});
 await page.goto(pathToFileURL(path.resolve('prototype/index.html')).href);
 const instruments=[
  {name:'SWAM Alto Sax',chineseName:'中音萨克斯',brand:'swam',pluginId:'old-swam',supported:true,isSwam:true},
  {name:'QinEngineV3',brand:'kong',pluginId:'old-kong',supported:true},
  {name:'Kontakt 8',brand:'kontakt',pluginId:'old-kontakt',supported:true},
  {name:'FengYin SF2',brand:'sf2',pluginId:'private-sf2',supported:true},
  {name:'Unknown',brand:'unsupported',supported:false}];
 await page.evaluate(instruments=>{
  document.querySelector('#license-lock').hidden=true;
  for(const f of window.listeners.backendState)f({activated:true,instruments,effects:[],pluginLoaded:false,deviceConnected:false});
 },instruments);
 const names=await page.locator('#instrument-select option').allTextContents();
 assert(names.includes('中音萨克斯'));assert(names.includes('民乐引擎'));assert(names.includes('sf2格式音源'));
 assert(names.some(x=>x==='Kontakt 8'));assert(names.every(x=>!/SWAM|QinEngine|FengYin SF2/i.test(x)));
 for(const i of [0,1,2,3]) {
  const selected=await page.locator(`#instrument-select option[value="${i}"]`).count();assert.equal(selected,1);
 }
 await page.evaluate(()=>{customToneMode='create';const select=document.querySelector('#instrument-select');select.value='3';select.dispatchEvent(new Event('change'));});
 assert.deepEqual(await page.evaluate(()=>window.sent.filter(x=>x[0]==='loadPlugin').at(-1)),['loadPlugin',{index:3}]);
 assert.deepEqual(errors,[]);console.log('Tuner Chinese labels, stable indices, Kontakt/unsupported preservation and SF2 selection passed');
 }finally{await browser.close()}
})().catch(e=>{console.error(e);process.exitCode=1});
