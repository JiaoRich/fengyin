const {chromium} = require('playwright');
const assert = require('node:assert/strict');
const {pathToFileURL} = require('node:url');
const path = require('node:path');
(async () => {
  const browser = await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  try {
    const page = await browser.newPage({viewport:{width:1920,height:1080}});
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.addInitScript(() => {
      window.listeners = {}; window.sent = [];
      window.__JUCE__ = {backend:{addEventListener:(n,f)=>window.listeners[n]=f,emitEvent:(n,p)=>window.sent.push({n,p})}};
    });
    await page.goto(pathToFileURL(path.resolve(__dirname,'../prototype/index.html')).href);
    await page.evaluate(() => {
      window.testState = {activated:true,pluginLoaded:true,pluginLoading:false,instruments:[],presets:[],
        pluginName:'SWAM Soprano Sax',pluginBrand:'swam',instrumentKey:'soprano-sax',instrumentChineseName:'高音萨克斯',
        bendPreferenceKey:'swam|soprano-sax',bendRange:1,bendRecommended:1,bendRangeApplied:true};
      window.listeners.backendState(window.testState);
    });
    assert.equal(await page.locator('#bend-range-value').innerText(),'±1');
    assert.match(await page.locator('#bend-hint').innerText(),/推荐弯音范围±1/);
    await page.locator('#bend-range-button').click();
    await page.locator('[data-bend="4"]').click();
    assert.match(await page.locator('#bend-confirm-message').innerText(),/高音萨克斯乐器的弯音范围修改为±4/);
    await page.locator('#confirm-bend-range').click();
    assert.deepEqual(await page.evaluate(()=>window.sent.filter(x=>x.n==='setBendRange').at(-1).p),
      {key:'swam|soprano-sax',value:4,reset:false});
    await page.evaluate(()=>window.listeners.bendRangeResult({success:false,message:'音源未支持'}));
    assert.equal(await page.locator('#bend-range-dialog').isVisible(),true);
    assert.equal(await page.locator('#bend-range-value').innerText(),'±1');
    await page.locator('#reset-bend-range').click();
    assert.equal(await page.evaluate(()=>window.sent.at(-1).p.reset),true);
    await page.evaluate(() => {
      window.listeners.bendRangeResult({success:true,message:'已恢复推荐值'});
      window.testState = {...window.testState,pluginName:'QinEngineV3',pluginBrand:'kong',instrumentKey:'container:erhu',
        instrumentChineseName:'二胡',bendPreferenceKey:'qin|erhu',bendRange:3,bendRecommended:3};
      window.listeners.backendState(window.testState);
    });
    assert.equal(await page.locator('#bend-range-value').innerText(),'±3');
    await page.locator('#bend-range-button').click();
    await page.evaluate(()=>window.listeners.backendState({...window.testState,bendPreferenceKey:'qin|dizi'}));
    assert.equal(await page.locator('#bend-range-dialog').isVisible(),false);
    for (const width of [1920,1366,1120]) {
      await page.setViewportSize({width,height:width===1920?1080:768});
      const bounds = await page.locator('#bend-range-button').boundingBox();
      assert(bounds && bounds.x>=0 && bounds.x+bounds.width<=width && bounds.y+bounds.height<=768+(width===1920?312:0));
    }
    await page.screenshot({path:'/private/tmp/fengyin-bend-ui.png'});
    assert.deepEqual(errors,[]);
    console.log('Bend range UI checks passed');
  } finally { await browser.close(); }
})().catch(e=>{console.error(e);process.exit(1)});
