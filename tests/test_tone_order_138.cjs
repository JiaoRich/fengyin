const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path');
const {pathToFileURL} = require('node:url');
const catalog = fs.readFileSync('native/Source/PublishedToneCatalog.h', 'utf8');
const tones = [...catalog.matchAll(/\{ "([^"]+)", "([^"]+)", "([^"]+)", "([^"]+)", PublishedToneKind::(\w+),/g)]
  .map(([, id, name, instrumentKey, styleId, kind]) => ({
    id, name, instrumentKey, styleId,
    kind: kind.startsWith('kong') ? 'kong' : kind.startsWith('kontakt') ? 'kontakt' : 'package',
  }));
const expected = tones.map(tone => 'builtin:' + tone.id);
const storageKey = 'fengyin-card-order-swam';

(async () => {
  const browser = await chromium.launch({
    headless:true, executablePath:process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  });
  try {
    const page = await browser.newPage({viewport:{width:1600,height:1000}});
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.addInitScript(() => {
      localStorage.setItem('fy-guide-seen', 'yes');
      window.listeners = {};
      window.sent = [];
      window.__JUCE__ = {backend:{
        addEventListener(name, listener) {
          const previous = window.listeners[name];
          window.listeners[name] = value => { previous?.(value); listener(value); };
        },
        emitEvent(name, payload) { window.sent.push({name,payload}); },
      }};
    });
    await page.goto(pathToFileURL(path.resolve('prototype/index.html')).href);
    const custom = {id:'studio-a',name:'测试方案',pluginId:'dummy',published:true};
    const state = {activated:true,featuresUnlocked:true,deviceConnected:false,
      instruments:[{brand:'kontakt',pluginId:'dummy'}],publishedTones:[...tones].reverse(),presets:[custom]};
    const emit = async (value = state) => page.evaluate(s => window.listeners.backendState(s), value);
    const show = async () => { await emit(); await page.locator('[data-page="sounds"]').click(); };
    const cards = page.locator('#preset-grid > [data-card-key]');
    const ids = () => cards.evaluateAll(nodes => nodes.map(node => node.dataset.cardKey));
    const readOrder = () => page.evaluate(key => JSON.parse(localStorage.getItem(key)), storageKey);

    await show();
    const defaultOrder = [...expected, 'custom:studio-a'];
    assert.deepEqual(await ids(), defaultOrder);
    assert.equal(await readOrder(), null, 'initial ordering never saves a user preference');
    assert.equal(await cards.locator('.card-drag-handle').count(), defaultOrder.length);
    await cards.first().locator('.card-drag-handle').click();
    await cards.first().locator('.card-drag-handle').dragTo(cards.nth(2).locator('h3'));
    const reordered = [expected[1], expected[2], expected[0], ...defaultOrder.slice(3)];
    assert.deepEqual(await ids(), reordered, 'mouse drag moves the requested card');
    assert.deepEqual(await readOrder(), reordered);
    assert.deepEqual(await page.evaluate(() => window.sent.filter(x => /^load(PublishedTone|Plugin|Preset)$/.test(x.name))), []);

    await page.reload();
    await show();
    assert.deepEqual(await ids(), reordered);
    await emit({...state,activeToneVariantId:expected[0]});
    assert.deepEqual(await ids(), reordered);
    assert.equal(await page.locator('#preset-grid .active').getAttribute('data-card-key'), expected[0]);
    await emit({...state,presets:[...state.presets,{...custom,id:'studio-new'}]});
    assert.deepEqual(await ids(), [...reordered, 'custom:studio-new']);
    assert.deepEqual(await readOrder(), reordered, 'refresh and new cards never rewrite the user preference');
    assert.deepEqual(await page.locator('#compact-tones button span').allTextContents(), await cards.locator('h3').allTextContents());
    const grid = page.locator('#preset-grid');
    const handle = cards.first().locator('.card-drag-handle');
    const outer = await grid.boundingBox(), grip = await handle.boundingBox();
    await page.mouse.move(grip.x + grip.width/2, grip.y + grip.height/2);
    await page.mouse.down();
    await page.mouse.move(outer.x + outer.width/3, outer.y + outer.height - 8,{steps:12});
    await page.waitForFunction(() => document.querySelector('#preset-grid').scrollTop > 35);
    await page.keyboard.press('Escape');
    await page.mouse.move(1, 1);
    await page.mouse.up();
    await page.waitForFunction(() => !document.querySelector('#preset-grid .dragging'));
    assert.deepEqual(await ids(), [...reordered, 'custom:studio-new'], 'cancel leaves display order unchanged');
    assert.deepEqual(await readOrder(), reordered, 'cancel leaves storage unchanged');

    await page.locator('[data-card-key="custom:studio-a"] .delete-tone').click();
    const events = () => page.evaluate(() => window.sent.filter(x => /^load(PublishedTone|Plugin|Preset)$|^deletePreset$/.test(x.name)));
    assert.deepEqual(await events(), [{name:'deletePreset',payload:{id:'studio-a'}}]);
    await page.locator('[data-card-key="builtin:kontakt-kensa"] h3').click();
    assert.deepEqual((await events()).at(-1),{name:'loadPublishedTone',payload:{id:'kontakt-kensa'}});

    await page.evaluate(key => localStorage.setItem(key,JSON.stringify([
      null,'builtin:kong-pipa','builtin:retired','builtin:trumpet:natural','builtin:kong-pipa',
    ])),storageKey);
    await page.reload();
    await show();
    const migrated = ['builtin:kong-pipa','builtin:trumpet'];
    assert.deepEqual(await ids(), [...migrated,...defaultOrder.filter(id => !migrated.includes(id))]);
    await page.evaluate(key => localStorage.setItem(key,'{}'),storageKey);
    await page.reload();
    await show();
    assert.deepEqual(await ids(),defaultOrder);
    assert.deepEqual(errors,[]);
    console.log(`PASS: ${tones.length} native catalogue defaults, drag, scroll, cancel, reload, refresh, added tone, legacy order, card actions`);
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exit(1); });
