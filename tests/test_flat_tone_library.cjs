const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync('prototype/app.js','utf8');
const body=source.slice(source.indexOf('function renderPresets()'),source.indexOf('\nrenderPresets();',source.indexOf('function renderPresets()')));
const elements={};const get=s=>elements[s]||(elements[s]={hidden:false,innerHTML:'',querySelectorAll:()=>[]});
const context={console,Date,toneLoadInFlight:false,pendingPresetNavigation:null,draggedInstrumentCardKey:'',availableInstruments:[],savedPresets:[],latestBackendState:{},
  localStorage:{getItem:()=>null},$ :get,$$:()=>[],bindInstrumentSorting:()=>{},escapeHtml:x=>String(x),
  toneStylesForInstrument:key=>Array.from({length:key==='alto-sax'?3:2},(_,i)=>({id:String(i),name:`tone${i}`}))};
vm.createContext(context);vm.runInContext(body,context);
context.renderPresets();
assert.equal((get('#preset-grid').innerHTML.match(/data-card-key=/g)||[]).length,7);
assert.equal((get('#preset-grid').innerHTML.match(/缺少音源/g)||[]).length,7);
context.availableInstruments=[{instrumentKey:'alto-sax',pluginId:'alto'},{instrumentKey:'violin',pluginId:'violin'}];
context.savedPresets=[{id:'old',name:'old',customTone:true,pluginId:'alto'},
 {id:'draft',name:'draft',published:true,studioDraft:true},
 {id:'published',name:'Published',published:true,pluginId:'alto'}];
context.renderPresets();
const html=get('#preset-grid').innerHTML;
assert.equal((html.match(/data-card-key=/g)||[]).length,8);
assert(!html.includes('builtin:violin'));
assert(!html.includes('custom:old'));assert(!html.includes('custom:draft'));
assert(html.includes('custom:published'));
assert.equal((html.match(/class="delete-tone"/g)||[]).length,1);
assert(!html.includes('scan-sounds'));assert(!html.includes('tone-next'));
console.log('Flat tone library: whitelist, missing dependencies, publication and deletion checks passed');
