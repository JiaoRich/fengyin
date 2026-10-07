const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const { pathToFileURL } = require('node:url');
const path = require('node:path');

(async () => {
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'
  });
  try {
    const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.addInitScript(() => {
      window.listeners = {};
      window.sent = [];
      window.__JUCE__ = { backend: {
        addEventListener: (name, callback) => {
          (window.listeners[name] ||= []).push(callback);
        },
        emitEvent: (name, payload) => window.sent.push({ name, payload })
      }};
      window.emitBackend = (name, value) => (window.listeners[name] || []).forEach(callback => callback(value));
    });
    await page.goto(pathToFileURL(path.resolve(__dirname, '../prototype/index.html')).href);
    await page.evaluate(() => window.emitBackend('backendState', {
      activated: true,
      deviceConnected: true,
      deviceMatched: true,
      deviceIdentifier: 'adapter-test',
      pluginLoaded: false,
      instruments: [],
      presets: [],
      publishedTones: [],
      techniqueMappings: [
        { id: 'vibrato', technique: 1, mode: 2, sourceType: 1, sourceNumber: 74, strength: .5 }
      ]
    }));
    await page.evaluate(() => showPage('wind'));
    assert.equal(await page.locator('.adapter-map-source').count(), 6);
    const hardware = await page.locator('#adapter-technique-grid').innerText();
    assert.match(hardware, /功能键 1/);
    assert.match(hardware, /功能键 2/);
    assert.match(hardware, /功能键 3/);
    assert.match(await page.locator('.adapter-map-source').first().innerText(), /CC74/);

    await page.evaluate(() => window.emitBackend('backendState', {
      activated: true,
      deviceConnected: true,
      deviceMatched: true,
      deviceIdentifier: 'adapter-test',
      pluginLoaded: true,
      pluginName: 'SWAM Violin',
      instrumentKey: 'violin',
      instrumentChineseName: '小提琴',
      instruments: [],
      presets: [],
      publishedTones: [],
      techniqueMappings: []
    }));
    await page.locator('[data-adapter-mode="breath"]').click();
    const breath = await page.locator('#adapter-technique-grid').innerText();
    assert.doesNotMatch(breath, /咬嘴／压力|体感控制|摇杆／拇指控制|功能键/);
    const toggles = page.locator('.adapter-technique-toggle input[type="checkbox"]');
    const strengths = page.locator('.adapter-technique-strength input[type="range"]');
    assert.ok(await toggles.count() > 0);
    assert.equal(await strengths.count(), await toggles.count());
    assert.deepEqual(errors, []);
    console.log('PASS: intelligent adapter hardware roles and breath controls.');
  } finally {
    await browser.close();
  }
})().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
