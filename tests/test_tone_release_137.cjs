const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync('prototype/app.js','utf8');
const body=source.slice(source.indexOf('function renderPresets()'),source.indexOf('\nrenderPresets();',source.indexOf('function renderPresets()')));
const rows=JSON.parse(fs.readFileSync('assets/tone-packages/release-137-audit.json','utf8')).tones;
const catalogue=rows.map(r=>({id:r.id,name:r.name,instrumentKey:r.instrumentKey,styleId:'release',kind:r.brand}));
const elements={};const get=s=>elements[s]||(elements[s]={hidden:false,innerHTML:'',querySelectorAll:()=>[]});
const context={console,Date,toneLoadInFlight:false,pendingPresetNavigation:null,draggedInstrumentCardKey:'',availableInstruments:[],savedPresets:[],publishedTones:catalogue,latestBackendState:{},localStorage:{getItem:()=>JSON.stringify(['builtin:violin-bright','builtin:kong-hulusi','builtin:kong-matouqin','builtin:kong-bangdi'])},$:get,$$:()=>[],bindInstrumentSorting:()=>{},escapeHtml:x=>String(x)};
vm.createContext(context);vm.runInContext(body,context);
context.renderPresets();
assert.equal((get('#preset-grid').innerHTML.match(/缺少音源/g)||[]).length,42);
context.availableInstruments=[...new Set(rows.filter(x=>x.brand==='swam').map(x=>x.instrumentKey))].map(instrumentKey=>({instrumentKey,brand:'swam'}));
context.availableInstruments.push({brand:'kong',pluginId:'qin'});
context.renderPresets();
assert.equal((get('#preset-grid').innerHTML.match(/缺少音源/g)||[]).length,2);
context.availableInstruments.push({brand:'kontakt',pluginId:'kontakt8'});
context.renderPresets();
const html=get('#preset-grid').innerHTML;
assert.equal((html.match(/data-card-key=/g)||[]).length,42);
assert(!html.includes('缺少音源'));
for(const r of rows){assert(html.includes(`data-card-key="builtin:${r.id}"`));assert(html.includes(`<h3>${r.name}</h3>`));}
for(const id of ['violin-bright','kong-hulusi','kong-matouqin'])assert(!html.includes(`data-card-key="builtin:${id}"`));
assert(html.indexOf('builtin:kong-bangdi')<html.indexOf('builtin:soprano-kenny')); // Keep user order for surviving identities.
console.log('Release 1.1.37: 42 cards, exact names, dependency states and retired-order migration PASS');
