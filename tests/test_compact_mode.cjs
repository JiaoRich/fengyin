const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const assert = require('node:assert/strict');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
(async () => {
  const browser = await chromium.launch({headless:true, executablePath:process.env.CHROME_PATH || undefined});
  try {
    const page = await browser.newPage({viewport:{width:600,height:800}});
    const errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.goto(pathToFileURL(path.resolve('prototype/index.html')).href);
    await page.evaluate(()=>{document.querySelector('#license-lock').hidden=true;applyWindowMode(true);});
    await page.waitForTimeout(300);
    assert.equal(await page.locator('#compact-tones button').count(),7);
    assert.equal(await page.locator('#compact-tones [draggable=true]').count(),0);
    assert.equal(await page.locator('.video-card').isVisible(),false);
    assert.equal(await page.locator('.sidebar').isVisible(),false);
    for(const theme of ['neon','spring','summer','autumn','winter','china-red','gold','minimal']){
      await page.selectOption('#theme',theme);
      assert.equal(await page.getAttribute('body','data-theme'),theme);
    }
    await page.selectOption('#theme','neon');
    for(const [width,height] of [[480,560],[480,700],[600,700],[680,700]]){
      await page.setViewportSize({width,height});
      assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'horizontal overflow');
      const rect=await page.locator('#theme').boundingBox();assert(rect && rect.x>=0 && rect.x+rect.width<=width);
      assert(rect.y+rect.height<=height,'theme control must fit vertically');
    }
    await page.waitForTimeout(500);
    await page.screenshot({path:'/private/tmp/fengyin-compact-qa.png'});
    await page.evaluate(()=>{applyWindowMode(false);showPage('sounds');latestBackendState.activeToneVariantId=document.querySelector('#preset-grid [data-card-key]').dataset.cardKey;currentPluginLoaded=true;latestBackendState.instrumentPeak=.7;renderPresets();});
    await page.setViewportSize({width:1400,height:900});
    await page.waitForTimeout(200);
    assert.equal(await page.locator('#preset-grid .instrument-preset-card').count(),7);
    assert.equal(await page.locator('#preset-grid .active').count(),1);
    assert(await page.locator('#preset-grid .active .tone-level').isVisible());
    await page.evaluate(()=>{latestBackendState.instrumentPeak=0;});
    await page.waitForTimeout(200);
    assert.equal(await page.locator('#preset-grid .active .tone-level i').first().evaluate(e=>e.style.transform),'scaleY(0.05)');
    const reverse=await page.locator('#preset-grid [data-card-key]').evaluateAll(es=>es.map(e=>e.dataset.cardKey).reverse());
    await page.evaluate(order=>{localStorage.setItem('fengyin-card-order-swam',JSON.stringify(order));renderPresets();applyWindowMode(true);},reverse);
    const fullNames=await page.locator('#preset-grid h3').allTextContents();
    assert.deepEqual(await page.locator('#compact-tones button span').allTextContents(),fullNames);
    assert.equal(await page.locator('.studio-names label').count(),1);
    assert.deepEqual(errors,[]);
    console.log('Compact UI: layout, no dragging, 8 themes, viewport bounds, unified studio name, runtime checks passed');
  } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
