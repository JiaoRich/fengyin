import {chromium} from '/Users/mengyang.jmy/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright/index.mjs';
import assert from 'node:assert/strict';
const browser=await chromium.launch({headless:true,channel:'chrome'});
try {
 for(const width of [1366,1920]){
  const page=await browser.newPage({viewport:{width,height:900}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>{
   window.handlers={};window.sent=[];
   window.dispatchNative=(name,payload)=>(window.handlers[name]||[]).forEach(fn=>fn(payload));
   window.__JUCE__={backend:{addEventListener:(name,fn)=>(window.handlers[name]??=[]).push(fn),emitEvent:(name,payload)=>window.sent.push({name,payload})}};
  });
  await page.goto(new URL('../prototype/index.html',import.meta.url).href);
  const instrument={name:'SWAM Soprano Sax 3',chineseName:'高音萨克斯',instrumentKey:'soprano-sax',brand:'swam',isSwam:true,isSupported:true,pluginId:'sax'};
  const state={pluginLoaded:true,pluginLoading:false,pluginBrand:'swam',pluginName:instrument.name,instrumentKey:instrument.instrumentKey,instrumentChineseName:instrument.chineseName,
   toneStyleId:'natural',activated:true,toneSettings:{air:0},instruments:[instrument],effects:['Test EQ','Test Compressor'],presets:[{id:'draft',name:'秘密草稿',studioDraft:true,instrumentKey:instrument.instrumentKey,brand:'swam',instrumentChineseName:instrument.chineseName}],effectChain:[]};
  const emit=async(name,payload)=>page.evaluate(({name,payload})=>window.dispatchNative(name,payload),{name,payload});
  await emit('backendState',state);
  await page.locator('.nav-item[data-page="sounds"]').click();
  assert.equal(await page.locator('.instrument-preset-card').count(),1);
  assert.equal(await page.locator('#preset-grid').getByText('秘密草稿').count(),0);
  assert.equal(await page.locator('#preset-grid').getByText('编辑',{exact:true}).count(),0);
  assert.equal(await page.locator('.nav-item[data-page="chain"]').isVisible(),false);
  await page.locator('.nav-item[data-page="settings"]').click();await page.locator('#studio-entry').click();
  await page.locator('#studio-password').fill('wrong');await page.locator('#studio-login-confirm').click();await emit('studioAuthResult',false);
  assert.match(await page.locator('#studio-login-status').textContent(),/密码不正确/);
  await page.locator('#studio-password').fill('admin');await page.locator('#studio-login-confirm').click();await emit('studioAuthResult',true);
  state.studioUnlocked=true;await emit('backendState',state);
  await page.locator('#studio-new').click();await page.locator('#instrument-select').selectOption('0');await emit('pluginLoadResult',{success:true});
  await page.locator('#studio-style-name').fill('空气独奏');await page.locator('#expert-air').fill('67');await page.locator('#expert-air').dispatchEvent('input');
  await emit('backendState',state);assert.equal(await page.locator('#expert-air').inputValue(),'67');
  await page.locator('#load-effect').click();assert.equal(await page.evaluate(()=>window.sent.at(-1).name),'loadEffect');
  state.effectChain=[{name:'Test EQ',version:'1.0',bypassed:false},{name:'Test Compressor',version:'2.0',bypassed:false}];await emit('backendState',state);
  await page.locator('[data-effect-action="up"]').nth(1).click();
  assert.deepEqual(await page.evaluate(()=>window.sent.at(-1).payload),{index:1,action:'up',bypassed:true});
  await page.locator('#save-expert').click();
  const payload=await page.evaluate(()=>window.sent.filter(e=>e.name==='saveCustomPreset').at(-1).payload);
  assert.equal(payload.name,'空气独奏');assert.equal(payload.instrumentName,'高音萨克斯');assert.equal(payload.settings.air,67);
  assert.equal(await page.locator('#save-expert').isDisabled(),true);await emit('studioSaveResult','draft');
  assert.equal(await page.locator('#save-expert').isDisabled(),false);
  await page.locator('#studio-export').click();await emit('studioSaveResult','draft');assert.equal(await page.evaluate(()=>window.sent.at(-1).name),'studioExport');
  await page.screenshot({path:`/tmp/fengyin-studio-${width}.png`,fullPage:true});
  await page.locator('#studio-show-drafts').click();assert.equal(await page.locator('.studio-draft-row').count(),1);
  await page.locator('[data-studio-edit]').click();await emit('presetLoadResult',{success:true});
  assert.equal(await page.locator('#instrument-select').isDisabled(),true);
  assert.equal(await page.locator('#studio-style-name').inputValue(),'秘密草稿');
  assert.deepEqual(errors,[]);console.log(`${width}: login/public isolation/edit/save/export/chain interactions passed`);
  await page.close();
 }
}finally{await browser.close();}
