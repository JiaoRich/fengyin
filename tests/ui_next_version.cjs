// Usage: NODE_PATH=<playwright packages> node tests/ui_next_version.cjs
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const { pathToFileURL } = require('node:url');
const path = require('node:path');
(async () => {
  const browser = await chromium.launch({ headless:true,
    executablePath:process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' });
  try {
    const page = await browser.newPage({viewport:{width:1920,height:1080}});
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.addInitScript(() => {
      window.listeners = {}; window.sent = [];
      window.__JUCE__ = {backend:{addEventListener:(name, callback) => window.listeners[name]=callback,
        emitEvent:(name, payload) => window.sent.push({name,payload})}};
    });
    await page.goto(pathToFileURL(path.resolve(__dirname,'../prototype/index.html')).href);
    await page.evaluate(() => {
      window.testState = {activated:true,deviceConnected:true,deviceMatched:true,deviceIdentifier:'test',
        pluginLoaded:false,techniqueMappings:[{id:'vibrato',technique:1,mode:2,sourceType:1,sourceNumber:74,strength:.5}],
        instruments:Array.from({length:18},(_,i)=>({brand:'swam',isSwam:true,name:`SWAM Test ${i}`,
          label:`SWAM Test ${i}`,chineseName:`测试乐器${i}`,instrumentKey:`test-${i}`})),presets:[]};
      window.listeners.backendState(window.testState);
      window.pushTimer = setInterval(()=>window.listeners.backendState({...window.testState,breath:Math.random()}),40);
    });
    await page.locator('[data-page="wind"]').click();
    assert.match(await page.locator('.adapter-map-source').first().innerText(),/CC74/);
    for (let i=0;i<30;i++) {
      await page.locator('.adapter-map-source').first().click({delay:110});
      await page.evaluate(()=>window.listeners.techniqueLearnResult({success:true,message:'映射成功'}));
    }
    assert.equal(await page.evaluate(()=>window.sent.filter(x=>x.name==='beginTechniqueLearn').length),30);
    await page.evaluate(()=>{ window.testState.pluginLoaded=true; window.testState.pluginName='SWAM Violin';
      window.testState.instrumentKey='violin'; window.testState.instrumentChineseName='小提琴';
      window.listeners.backendState(window.testState); });
    await page.locator('[data-page="play"]').click();
    await page.locator('#technique-settings').click();
    for (let i=0;i<30;i++) {
      await page.locator('#technique-list .learn-source').first().click({delay:110});
      await page.evaluate(()=>window.listeners.techniqueLearnResult({success:true,message:'映射成功'}));
    }
    assert.equal(await page.evaluate(()=>window.sent.filter(x=>x.name==='beginTechniqueLearn').length),60);
    await page.locator('#close-technique-dialog-top').click();
    await page.locator('[data-page="sounds"]').click();
    await page.waitForTimeout(100);
    const columns = await page.locator('#preset-grid').evaluate(el=>getComputedStyle(el).gridTemplateColumns.split(' ').length);
    assert.equal(columns,3,'1920 width must show three columns');
    const beforeOrder = await page.locator('.instrument-preset-card').evaluateAll(nodes=>nodes.map(n=>n.dataset.cardKey));
    await page.locator('.instrument-preset-card header').nth(0).dragTo(page.locator('.instrument-preset-card header').nth(2));
    const afterOrder = await page.locator('.instrument-preset-card').evaluateAll(nodes=>nodes.map(n=>n.dataset.cardKey));
    assert.notDeepEqual(afterOrder,beforeOrder,'drag must reorder cards');
    assert.deepEqual(await page.evaluate(()=>JSON.parse(localStorage.getItem('fengyin-card-order-swam'))),afterOrder);
    const scrollBefore = await page.evaluate(()=>{
      const grid=document.querySelector('#preset-grid'); let host=grid.parentElement;
      while(host && !(/(auto|scroll)/.test(getComputedStyle(host).overflowY) && host.scrollHeight>host.clientHeight)) host=host.parentElement;
      window.testScrollHost=host||document.scrollingElement;
      const card=grid.querySelector('.instrument-preset-card'); window.dragSource=card; window.testTransfer=new DataTransfer();
      card.dispatchEvent(new DragEvent('dragstart',{bubbles:true,dataTransfer:window.testTransfer,clientX:500,clientY:500}));
      const edge=window.testScrollHost===document.scrollingElement?innerHeight:window.testScrollHost.getBoundingClientRect().bottom;
      document.dispatchEvent(new DragEvent('dragover',{bubbles:true,dataTransfer:window.testTransfer,clientX:500,clientY:Math.min(innerHeight,edge)-10}));
      return window.testScrollHost.scrollTop;
    });
    await page.waitForTimeout(600);
    assert.ok(await page.evaluate(()=>window.testScrollHost.scrollTop)>scrollBefore,'edge drag must auto-scroll');
    await page.evaluate(()=>window.dragSource.dispatchEvent(new DragEvent('dragend',{bubbles:true,dataTransfer:window.testTransfer})));
    assert.deepEqual(await page.locator('.instrument-preset-card').evaluateAll(nodes=>nodes.map(n=>n.dataset.cardKey)),afterOrder,'cancel must restore order');
    for (const width of [1536,1280,1120]) {
      await page.setViewportSize({width,height:800});
      await page.waitForTimeout(100);
      const clipped = await page.locator('.instrument-preset-card h3').evaluateAll(nodes=>nodes.some(n=>n.scrollWidth>n.clientWidth+1));
      assert.equal(clipped,false,`titles clipped at ${width}`);
    }
    await page.locator('[data-action="create"]').click();
    assert.equal(await page.locator('#instrument-select').inputValue(),'');
    assert.equal(await page.locator('#instrument-load-state').isVisible(),false);
    await page.screenshot({path:'/private/tmp/fengyin-custom-ui.png',fullPage:true});
    await page.evaluate(()=>{
      window.testState.instruments=[{brand:'kong',name:'QinEngineV3',pluginId:'qin-test'}];
      window.testState.kongLibrary={ready:false,exists:false,fileCount:0,path:'',source:'',instruments:[]};
      window.testState.kongInstruments=[];
      window.listeners.backendState(window.testState);
    });
    await page.locator('[data-page="sounds"]').click();
    assert.equal(await page.evaluate(()=>window.sent.filter(x=>x.name==='chooseKongLibrary').length),1,
      'first visit to Kong library must ask for the folder once');
    await page.evaluate(()=>{
      window.testState.kongLibrary={ready:true,exists:true,fileCount:2,path:'D:\\音乐 & 自选库',source:'selected',
        instruments:[{key:'kong-erhu-2',name:'二胡二',file:'ErHu_II.KAI',recognised:true},{key:'kong-kai-abc',name:'New Instrument',file:'New_Instrument.KAI',recognised:false}]};
      window.listeners.backendState(window.testState);
    });
    assert.equal(await page.locator('[data-action="kong-library"]').innerText(),'更换音色库目录');
    assert.ok((await page.locator('#preset-grid').innerText()).includes('D:\\音乐 & 自选库'));
    await page.locator('[data-action="kong-library"]').click();
    assert.equal(await page.evaluate(()=>window.sent.filter(x=>x.name==='chooseKongLibrary').length),2);
    assert.equal(await page.locator('.instrument-preset-card').count(),0,
      'KAI inventory must not create misleading playable instrument cards');
    await page.locator('[data-action="add-container-instrument"]').click();
    assert.equal(await page.evaluate(()=>window.sent.filter(x=>x.name==='beginContainerInstrument').length),1);
    await page.evaluate(()=>window.listeners.containerInstrumentResult({success:true,stage:'ready',message:'请选择乐器'}));
    await page.locator('#container-instrument-name').fill('二胡');
    await page.locator('#confirm-container-instrument').click();
    assert.deepEqual(await page.evaluate(()=>window.sent.filter(x=>x.name==='commitContainerInstrument').at(-1)?.payload),{name:'二胡'});
    await page.evaluate(()=>window.listeners.containerInstrumentResult({success:true,stage:'saved',message:'已添加：二胡'}));
    await page.evaluate(()=>{
      window.testState.presets=[{id:'kong-user-1',name:'原厂音色',brand:'kong',pluginId:'qin-test',
        instrumentKey:'container:kong-v3:kong-user-1',instrumentChineseName:'二胡',containerInstrument:true,containerAdapter:'kong-v3'}];
      window.listeners.backendState(window.testState);
    });
    assert.equal(await page.locator('.instrument-preset-card').count(),1,'only user-created Qin instruments become cards');
    assert.equal(await page.locator('.instrument-preset-card h3').innerText(),'二胡');
    assert.equal(await page.locator('.instrument-preset-card button').filter({hasText:'待适配'}).count(),0);
    await page.evaluate(()=>{
      window.testState.pluginLoaded=true; window.testState.pluginBrand='kong';
      window.testState.instrumentKey='container:kong-v3:kong-user-1'; window.testState.instrumentChineseName='二胡';
      window.testState.instrumentModels=[]; window.listeners.backendState(window.testState);
    });
    await page.locator('[data-page="play"]').click();
    assert.equal(await page.locator('#instrument-model-switcher').isVisible(),false,
      'Kong instruments must not show the SWAM model selector');
    assert.match(await page.locator('#instrument-picture').getAttribute('src'),/instrument_kong_erhu\.png$/);
    await page.evaluate(()=>{window.testState.kongLibrary.ready=false;window.testState.kongLibrary.exists=false;
      window.listeners.backendState(window.testState);});
    assert.ok((await page.locator('#preset-grid').innerText()).includes('原音色库目录不可用'));
    assert.deepEqual(errors,[]);
    console.log('PASS: 60 mapping clicks (inline + modal) under 25Hz refresh, mapping without plugin, 3 columns, drag/reorder/persistence, edge scroll, cancel rollback, adaptive titles, blank custom source.');
  } finally { await browser.close(); }
})().catch(error=>{console.error(error);process.exitCode=1;});
