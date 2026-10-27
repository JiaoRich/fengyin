const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => [...document.querySelectorAll(selector)];
function setTextIfChanged(element, value) {
  if (element && element.textContent !== String(value)) element.textContent = String(value);
}
let backendRefreshTimer=0;
const meterOnlyNativeEvents=new Set(['requestBackendState','webReady','nativeVideoViewport',
  'windowDragRegion','windowAction','syncVideoPlayback']);
// A new document must not inherit a native video window's stale visibility.
const videoWebSession = `${Date.now()}-${Math.random().toString(36).slice(2)}`;
let nativeViewportFrame=0,nativeViewportLast='',nativeViewportEpoch=0,nativeViewportRevision=0;
let studioUnlocked = false, studioSavedId = '', studioExportAfterSave = false;
let compactMode = false;
let touchWindowGesture = null;
let windowGestureMoveTimer=null;
function flushWindowGestureMove() {
  if(windowGestureMoveTimer!==null)clearTimeout(windowGestureMoveTimer);
  windowGestureMoveTimer=null;
  if(!touchWindowGesture?.delta)return;
  const [x,y]=touchWindowGesture.delta;touchWindowGesture.delta=null;
  nativeEvent('windowAction',{action:`pointer-move:${x}:${y}`});
}
function beginTouchWindowGesture(e, action) {
  if (e.isPrimary === false || touchWindowGesture) return true;
  // Mouse movement belongs to the native desktop, not WebView IPC. A moving
  // WebView must not own mouse capture or feed CSS screen deltas back to itself.
  if(e.pointerType==='mouse') {
    nativeEvent('windowAction',{action});
    return true;
  }
  touchWindowGesture = {id:e.pointerId,x:e.screenX,y:e.screenY,target:e.currentTarget};
  try { e.currentTarget.setPointerCapture(e.pointerId); }
  catch { touchWindowGesture=null; return true; }
  nativeEvent('windowAction',{action:`pointer-begin:${action}`});
  return true;
}
document.addEventListener('pointermove',e=>{
  if (!touchWindowGesture || e.pointerId!==touchWindowGesture.id) return;
  e.preventDefault();
  touchWindowGesture.delta=[Math.round(e.screenX-touchWindowGesture.x),Math.round(e.screenY-touchWindowGesture.y)];
  if(windowGestureMoveTimer===null)windowGestureMoveTimer=setTimeout(flushWindowGestureMove,1000/60);
},{passive:false});
for(const event of ['pointerup','pointercancel','lostpointercapture']) window.addEventListener(event,e=>{
  if (!touchWindowGesture || e.pointerId!==touchWindowGesture.id) return;
  if(event==='pointerup')flushWindowGestureMove();
  cancelWindowGesture();
},true);
function cancelWindowGesture() {
  const gesture=touchWindowGesture;
  if(!gesture)return;
  if(windowGestureMoveTimer!==null)clearTimeout(windowGestureMoveTimer);
  windowGestureMoveTimer=null;
  touchWindowGesture=null;
  if(gesture.target.hasPointerCapture?.(gesture.id))gesture.target.releasePointerCapture(gesture.id);
  nativeEvent('windowAction',{action:'pointer-end'});
}
window.addEventListener('blur',cancelWindowGesture);
document.addEventListener('visibilitychange',()=>{if(document.hidden)cancelWindowGesture();});
// Keep page scale fixed, not window size. Ordinary one-finger scrolling and
// sliders remain usable; compact resizing remains available at the edges.
document.addEventListener('touchstart',e=>{if(e.touches.length>1){cancelWindowGesture();e.preventDefault();}},{passive:false});
for(const type of ['gesturestart','gesturechange'])document.addEventListener(type,e=>e.preventDefault(),{passive:false});
document.addEventListener('wheel',e=>{if(e.ctrlKey)e.preventDefault();},{passive:false});
const windowControls = document.createElement('div');
windowControls.className = 'compact-window-controls';
windowControls.innerHTML = '<button type="button" aria-label="最小化" data-action="minimise">−</button><button type="button" aria-label="关闭" data-action="close">×</button>';
$('.window-mode-bar').appendChild(windowControls);
windowControls.addEventListener('click', e => {
  const action = e.target.closest('[data-action]')?.dataset.action;
  if (action) nativeEvent('windowAction', {action});
});
$('.window-mode-bar').addEventListener('pointerdown', e => {
  if (!compactMode || e.button !== 0 || e.target.closest('button,input,select,a')) return;
  e.preventDefault();
  if (beginTouchWindowGesture(e,'drag')) return;
  nativeEvent('windowAction', {action:'drag'});
});
// Capture resize gestures inside the WebView without recreating its native peer.
for (const edge of ['n','s','e','w','ne','nw','se','sw']) {
  const handle = document.createElement('div');
  handle.className = `compact-resize-handle resize-${edge}`;
  handle.setAttribute('aria-hidden', 'true');
  handle.addEventListener('pointerdown', e => {
    if (!compactMode || e.button !== 0) return;
    e.preventDefault(); e.stopPropagation();
    if (beginTouchWindowGesture(e,`resize-${edge}`)) return;
    nativeEvent('windowAction', {action:`resize-${edge}`});
  });
  document.body.appendChild(handle);
}
function applyWindowMode(compact) {
  cancelWindowGesture();
  if(compact) setNativeVideoExpanded(false);
  compactMode = !!compact;
  document.body.classList.toggle('compact-mode', compactMode);
  $('#window-mode-toggle').textContent = compactMode ? '全屏' : '精简模式';
  (compactMode ? $('.window-mode-bar') : $('.system-pills')).appendChild($('#window-mode-toggle'));
  (compactMode ? $('.window-mode-bar') : $('.system-pills')).appendChild(windowControls);
  closeQuickTones();
  if (compactMode) showPage('play');
  updateVideoPresentation();
  requestVisualFrame();
  requestAnimationFrame(reportNativeDragRegion);
}
function reportNativeDragRegion() {
  const bar=$('.window-mode-bar'),r=bar.getBoundingClientRect();
  const buttons=[...bar.querySelectorAll('button')].map(b=>b.getBoundingClientRect()).filter(b=>b.width>0);
  const right=Math.min(r.right,...buttons.map(b=>b.left))-4;
  nativeEvent('windowDragRegion',{x:Math.ceil(r.left),y:Math.ceil(r.top),width:compactMode?Math.max(0,Math.floor(right-r.left)):0,height:Math.floor(r.height)});
}
new ResizeObserver(reportNativeDragRegion).observe($('.window-mode-bar'));
$('#window-mode-toggle').onclick = () => nativeEvent('setCompactMode', {compact:!compactMode});
$('.system-pills').appendChild($('#window-mode-toggle'));
$('.system-pills').appendChild(windowControls);
window.__JUCE__?.backend?.addEventListener('windowModeChanged', applyWindowMode);
document.addEventListener('keydown', e => {
  if(e.key==='Escape' && document.body.classList.contains('video-expanded')) {
    e.preventDefault();setNativeVideoExpanded(false);return;
  }
  if (e.key === 'Escape' && !compactMode && !document.querySelector('[role="dialog"]:not([hidden])'))
    nativeEvent('setCompactMode', {compact:true});
});

const titles = {play:'开始演奏',sounds:'音色方案',chain:'调音师工作台',wind:'智能适配',audio:'高级设置',settings:'高级设置'};
const presets = [
  ['高音萨克斯','SWAM Soprano Sax 3','自然原声、丝滑抒情、明亮舞台'],
  ['中音萨克斯','SWAM Alto Sax 3','自然原声、温暖爵士、流行穿透'],
  ['次中音萨克斯','SWAM Tenor Sax 3','自然原声、烟熏爵士、深情抒情'],
  ['上低音萨克斯','SWAM Baritone Sax 3','自然原声、厚重爵士、流行低音'],
  ['小号','SWAM Trumpet','自然原声、明亮流行、柔和抒情'],
  ['C调小号','SWAM Trumpet (C)','自然原声、明亮舞台、交响铜管'],
  ['高音小号','SWAM Piccolo Trumpet','自然原声、巴洛克、明亮庆典'],
  ['柔音号','SWAM Flugelhorn','自然原声、温暖爵士、柔和抒情'],
  ['降E调柔音号','SWAM Flugelhorn Eb','自然原声、圆润铜管、电影抒情'],
  ['次中音长号','SWAM Tenor Trombone','自然原声、爵士铜管、雄壮舞台'],
  ['低音长号','SWAM Bass Trombone','自然原声、厚重交响、电影史诗'],
  ['中音长号','SWAM Alto Trombone','自然原声、古典铜管、温暖抒情'],
  ['上低音号','SWAM Euphonium','自然原声、温暖歌唱、铜管乐团'],
  ['F调圆号','SWAM French Horn','自然原声、温暖交响、电影史诗'],
  ['低音大号','SWAM Bass Tuba','自然原声、深沉交响、宏大电影'],
  ['长笛','SWAM Flute','自然原声、通透明亮、空灵抒情'],
  ['短笛','SWAM Piccolo','自然原声、轻快明亮、交响高音'],
  ['中音长笛','SWAM Alto Flute','自然原声、温暖抒情、空灵氛围'],
  ['低音长笛','SWAM Bass Flute','自然原声、深沉气息、电影氛围'],
  ['单簧管','SWAM Clarinet','自然原声、温暖爵士、古典抒情'],
  ['低音单簧管','SWAM Bass Clarinet','自然原声、烟熏低沉、电影悬疑'],
  ['双簧管','SWAM Oboe','自然原声、古典抒情、电影叙事'],
  ['英国管','SWAM English Horn','自然原声、温暖忧郁、电影叙事'],
  ['巴松管','SWAM Bassoon','自然原声、古典温暖、轻松谐谑'],
  ['倍低音巴松','SWAM Contrabassoon','自然原声、深沉交响、电影暗色'],
  ['小提琴独奏','SWAM Violin','自然独奏、温暖抒情、电影叙事']
  ,['中提琴独奏','SWAM Viola','自然独奏、温暖抒情、室内乐叙事']
  ,['大提琴独奏','SWAM Cello','自然独奏、深情抒情、电影史诗']
  ,['低音提琴独奏','SWAM Double Bass','自然独奏、温暖低音、电影厚重']
];

const kongPresets = [
  ['竹笛','kong-dizi','笛','自然原声、清亮丝竹、空灵山水'],
  ['洞箫','kong-xiao','箫','深沉自然、空灵山水、古风叙事'],
  ['葫芦丝','kong-hulusi','丝','自然甜美、柔和抒情、明亮民歌'],
  ['巴乌','kong-bawu','乌','自然原声、温暖抒情、深情民歌'],
  ['唢呐','kong-suona','呐','自然原声、喜庆明亮、厚重史诗'],
  ['管子','kong-guanzi','管','自然原声、苍凉叙事、高亢舞台'],
  ['笙','kong-sheng','笙','自然原声、古雅和鸣、空灵氛围'],
  ['埙','kong-xun','埙','自然原声、苍茫叙事、远古氛围'],
  ['二胡','kong-erhu','胡','自然原声、温暖抒情、苍凉叙事'],
  ['高胡','kong-gaohu','高','自然原声、岭南明亮、流行抒情'],
  ['京胡','kong-jinghu','京','自然原声、京剧高亢、戏曲舞台'],
  ['马头琴','kong-matouqin','马','自然原声、草原宽广、苍凉叙事'],
  ['琵琶','kong-pipa','琵','自然原声、清脆独奏、战场叙事'],
  ['古筝','kong-guzheng','筝','自然原声、古风清雅、电影叙事'],
  ['古琴','kong-guqin','琴','自然原声、幽远古雅、静谧山水'],
  ['中阮','kong-ruan','阮','自然原声、温润弹拨、民乐合奏']
];

let simulating = false;
let recording = false;
let screenRecordingBusy=false;
let animationFrame;
let visualTimer = null;
let videoUrl;
let selectedVideoFile;
let videoGeneration = 0;
let videoLoading = false;
let videoWantsPlaying = false;
let videoPlayRequest = 0;
let triedVideoDataFallback = false;
let nativeAccompanimentReady = false;
let nativeVideoEngine = false;
let nativeVideoDuration = 0, nativeVideoPosition = 0, nativeVideoLastError = '';
let lastVideoSyncAt = 0;
let videoSeekDragging = false;
let pendingVideoSeek = null;
let videoSeekRevision=0;
let nativeVideoFinished=false;
let preparedVideoSource=null;
let restoringVideoPosition=null;
let originalVideoSource='';
let usingPreparedVideo=false;
let videoMetricsBaseline=null;
let videoClockCorrections=0;
let availableInstruments = [];
let availableEffects = [];
let pluginListSignature = '';
let savedPresets = [];
let presetListSignature = '';
let hardwareBreath = null;
let compactRimBreath = 0;
let currentArtworkKey = '';
let currentInstrument = null;
let bendState = {pluginLoaded:false}, bendDialogKey = '', bendSelection = 0, bendBusy = false;
let bendHintTimer = null, lastBendKey = '';
let currentPluginLoaded = false;
let pendingPresetNavigation = null;
let publishedTones = [];
let toneLoadInFlight=false, queuedToneLoad=null;
let lastToneDragAt=0;
let customToneMode = 'idle';
let customSourceReady = false;
let editingPresetName = '';
let techniqueMappings = [];
let techniquePlan = [];
let globalTechniqueMode = 'breath';
let latestBackendState = {};
let appFocused = document.hasFocus();
let unfocusedFrame = 0;
let selectedTargetKey = 0;
let techniqueSensitivity = 'standard';
let lastDrawAt = 0;
let currentToneStyleIndex = 0;
let currentToneStyleInstrument = '';
let currentInstrumentModelIndex = 0;
let currentInstrumentModelKey = '';
let activeLibraryTab = 'swam';
const presetCardToneIndex = new Map();
let draggedInstrumentCardKey = '';
let kongLibraryPrompted = false;
let adapterMatchTimer = 0;
let adapterMode = 'hardware';
let adapterAutoOpenedFor = '';
let adapterMatchPhase = 'idle';
let expertSettings = null;
let expertDirty = false;
let reverbDragging = false;
let techniqueRenderSignature = '';
let adapterRenderSignature = '';

const toneStyleLibrary = {
  "soprano-sax": [
    {
      "id": "kenny",
      "name": "原声",
      "description": "",
      "eq": 0.0,
      "reverb": 34.4393998384476,
      "warmth": 62.6179993152618
    },
    {
      "id": "pop-high",
      "name": "明亮",
      "description": "",
      "eq": 21.814000606536872,
      "reverb": 38.95620107650757,
      "warmth": 62.61799931526184
    }
  ],
  "alto-sax": [
    {
      "id": "jazz",
      "name": "爵士",
      "description": "",
      "eq": 0.0,
      "reverb": 18.0,
      "warmth": 10.0
    },
    {
      "id": "deep",
      "name": "深情",
      "description": "",
      "eq": 20.0,
      "reverb": 32.0,
      "warmth": 10.0
    },
    {
      "id": "pop",
      "name": "流行",
      "description": "",
      "eq": 40.0,
      "reverb": 32.0,
      "warmth": 10.0
    }
  ],
  "tenor-sax": [
    {
      "id": "mellow",
      "name": "醇厚",
      "description": "",
      "eq": -50.0,
      "reverb": 22.0,
      "warmth": 32.0
    },
    {
      "id": "warm",
      "name": "温暖",
      "description": "",
      "eq": 5.0,
      "reverb": 12.0,
      "warmth": 16.0
    }
  ]
};

const instrumentArtwork = {
  'soprano-sax':'instrument_soprano_sax.png', 'alto-sax':'instrument_alto_sax.png',
  'tenor-sax':'instrument_tenor_sax.png', 'baritone-sax':'instrument_baritone_sax.png',
  'flugelhorn-eb':'instrument_flugelhorn.png', 'flugelhorn':'instrument_flugelhorn.png',
  'trumpet':'instrument_trumpet.png', 'trumpet-c':'instrument_trumpet.png',
  'piccolo-trumpet':'instrument_piccolo_trumpet.png',
  'double-bass-trombone':'instrument_bass_trombone.png', 'bass-trombone':'instrument_bass_trombone.png',
  'tenor-bass-trombone':'instrument_tenor_trombone.png', 'tenor-trombone':'instrument_tenor_trombone.png',
  'alto-trombone':'instrument_tenor_trombone.png', 'bass-tuba':'instrument_tuba.png',
  'tuba-eb':'instrument_tuba.png', 'euphonium':'instrument_euphonium.png',
  'horn-f':'instrument_horn.png', 'horn-bb':'instrument_horn.png',
  'piccolo':'instrument_piccolo.png', 'flute':'instrument_flute.png',
  'alto-flute':'instrument_alto_flute.png', 'bass-flute':'instrument_bass_flute.png',
  'clarinet':'instrument_clarinet.png', 'bass-clarinet':'instrument_bass_clarinet.png',
  'oboe':'instrument_oboe.png', 'english-horn':'instrument_english_horn.png',
  'bassoon':'instrument_bassoon.png', 'contrabassoon':'instrument_contrabassoon.png',
  'violin':'instrument_violin.png', 'violin-section':'instrument_violin.png',
  'viola':'instrument_viola.png', 'viola-section':'instrument_viola.png',
  'cello':'instrument_cello.png', 'cello-section':'instrument_cello.png',
  'double-bass':'instrument_double_bass.png', 'double-bass-section':'instrument_double_bass.png'
};

const kongInstrumentArtwork = {
  'kong-banhu':'instrument_kong_banhu.png', 'kong-erhu':'instrument_kong_erhu.png',
  'kong-erhu-2':'instrument_kong_erhu.png', 'kong-gaohu':'instrument_kong_erhu.png',
  'kong-jinghu':'instrument_kong_banhu.png', 'kong-zhonghu':'instrument_kong_erhu.png',
  'kong-matouqin':'instrument_kong_erhu.png', 'kong-guzheng':'instrument_kong_guzheng.png',
  'kong-guzheng-2':'instrument_kong_guzheng.png', 'kong-dizi':'instrument_kong_dizi.png',
  'kong-dizi-2':'instrument_kong_dizi.png', 'kong-xiao':'instrument_kong_xiao.png',
  'kong-nanxiao':'instrument_kong_xiao.png', 'kong-pipa':'instrument_kong_pipa.png',
  'kong-suona':'instrument_kong_suona.png', 'kong-suona-2':'instrument_kong_suona.png',
  'kong-hulusi':'instrument_kong_hulusi.png', 'kong-hulusi-2':'instrument_kong_hulusi.png',
  'kong-bawu':'instrument_kong_dizi.png', 'kong-guanzi':'instrument_kong_dizi.png',
  'kong-yangqin':'instrument_kong_yangqin.png', 'kong-guqin':'instrument_kong_guqin.png',
  'kong-sheng':'instrument_kong_sheng.png', 'kong-ruan':'instrument_kong_ruan.png',
  'kong-ruan-2':'instrument_kong_ruan.png', 'kong-liuqin':'instrument_kong_liuqin.png',
  'kong-sanxian':'instrument_kong_sanxian.png', 'kong-xun':'instrument_kong_xun.png'
};

const techniqueLibrary = {
  saxophone: [
    ['vibrato','颤音','≈','让长音自然起伏','auto','自动：控制器或自然延迟渐入',62,true],
    ['portamento','滑音与连奏','⌁','根据换音速度和音符衔接自动判断','auto','自动：音符关系智能',48,true],
    ['growl','嘶吼音','≋','强吹后继续加气，嘶吼逐渐加入','breath','气息智能：强吹渐入',42,true],
    ['flutter','花舌','✦','适合强调句和特殊音色','hardware','推荐：按键允许，气息控制深浅',35,true],
    ['fall','下滑音与收尾','↘','根据句尾收气动作辅助触发','breath','气息动作：句尾快速收气',40,false],
    ['overblow','泛音与超吹','△','强奏时增加泛音色彩','breath','气息智能：仅在强奏区加入',32,false],
    ['breathNoise','气声','◌','弱吹时增加空气质感','breath','气息智能：弱吹区自然加入',28,false]
  ],
  flute: [
    ['vibrato','颤音','≈','长音自然渐入，不影响快速乐句','auto','自动：控制器或自然渐入',58,true],
    ['flutter','花舌','✦','适合强调音和现代演奏效果','hardware','推荐：按键允许，气息控制深浅',36,true],
    ['breathNoise','气声','◌','随气息增加真实空气质感','breath','气息智能：连续跟随',34,true],
    ['overblow','超吹','△','强吹时进入明亮泛音区','breath','气息动作：强奏区渐入',38,true],
    ['portamento','滑音','⌁','根据连奏关系平滑换音','auto','自动：音符关系智能',42,false],
    ['altFingering','替代指法音色','◇','增加同音异色变化','hardware','推荐：功能键切换',35,false]
  ],
  woodwind: [
    ['vibrato','颤音','≈','长音自然起伏','auto','自动：控制器或自然渐入',55,true],
    ['portamento','滑音与连奏','⌁','自动识别连奏和音程','auto','自动：音符关系智能',42,true],
    ['breathNoise','气声与噪声','◌','根据乐器特性补充真实细节','breath','气息智能：连续跟随',28,true],
    ['growl','特殊共鸣','≋','仅在音源支持时显示','hybrid','组合：硬件允许＋气息深度',30,false],
    ['altFingering','替代指法音色','◇','增加同音异色变化','hardware','推荐：功能键切换',30,false]
  ],
  brass: [
    ['vibrato','唇颤音','≈','适合长音和抒情乐句','auto','自动：控制器或自然渐入',52,true],
    ['growl','咆哮音','≋','强奏时增加粗粝感','breath','气息智能：强吹渐入',40,true],
    ['flutter','花舌','✦','用于强调和特殊效果','hybrid','组合：硬件允许＋气息深度',34,true],
    ['fall','Doit／Fall','↗','识别句尾动作或由按键触发','auto','自动：句尾动作识别',40,true],
    ['mute','弱音器','●','在开放音色与弱音器之间切换','hardware','推荐：功能键切换',50,false],
    ['halfValve','半按阀效果','◒','模拟铜管半按阀音色','hardware','推荐：连续控制器',32,false],
    ['portamento','滑音','⌁','适合长号及特殊铜管乐句','auto','自动：音符关系智能',44,false]
  ],
  strings: [
    ['vibrato','揉弦','≈','长音自动渐入，也可由控制器连续控制','auto','自动：长音渐入',56,true],
    ['legato','连奏','⌒','根据音符衔接自动选择弓法','auto','自动：音符关系智能',60,true],
    ['portamento','滑音','⌁','按音程和换音速度智能判断','auto','自动：连奏与速度识别',44,true],
    ['bowPressure','弓压','╱','气息越强，弓压与力度越明显','breath','气息智能：连续跟随',48,true],
    ['pizzicato','拨奏','•','在拉弓和拨奏之间切换','hardware','推荐：功能键切换',50,false],
    ['tremolo','颤弓','≡','切换或连续控制颤弓强度','hybrid','组合：硬件允许＋气息强度',38,false]
  ]
};

function instrumentFamily(key = '') {
  if (key.startsWith('kong-') || key.startsWith('container:kong-v3:')) {
    if (key.startsWith('kong-suona')) return 'brass';
    if (['kong-dizi','kong-dizi-2','kong-xiao','kong-nanxiao','kong-xun','kong-guanzi','kong-hulusi','kong-hulusi-2','kong-bawu','kong-sheng'].includes(key)) return 'woodwind';
    if (key !== 'kong-misc') return 'strings';
    return 'woodwind';
  }
  if (key.includes('sax')) return 'saxophone';
  if (key.includes('flute') || key === 'piccolo') return 'flute';
  if (['clarinet','bass-clarinet','oboe','english-horn','bassoon','contrabassoon'].includes(key)) return 'woodwind';
  if (['violin','viola','cello','double-bass'].includes(key)) return 'strings';
  return 'brass';
}

function buildTechniquePlan(key = '') {
  const kong = key.startsWith('kong-');
  const other = {violin:'抖弓',cello:'抖弓',flute:'花舌',piccolo:'花舌'}[key];
  const folkVibrato = ['kong-erhu','kong-erhu-2','kong-matouqin','kong-hulusi','kong-hulusi-2','kong-xun','kong-nanxiao','kong-xiao','kong-dizi','kong-dizi-2','kong-suona','kong-suona-2','kong-jinghu'].includes(key);
  return [
    {id:'vibrato',name:/violin|viola|cello|double-bass|kong-erhu|kong-matouqin|kong-jinghu/.test(key)?'揉弦':'颤音',icon:'≈',index:1,pluginSupported:key==='kontakt-kensa' || folkVibrato || /sax|violin|viola|cello|double-bass|flute|piccolo|clarinet|oboe|english-horn|bassoon|trumpet|trombone|flugelhorn|tuba|euphonium|horn-f|horn-bb/.test(key)},
    {id:'growl',name:'嘶吼音',icon:'≋',index:0,pluginSupported:['alto-sax','tenor-sax','baritone-sax'].includes(key)},
    {id:'mute',name:kong?'技巧音色':other?`其他技巧：${other}`:'其他技巧',icon:'✦',index:8,pluginSupported:kong || !!other}
  ].map(item=>({...item,description:item.id==='mute'?(kong?'按住加入技巧音色，松开结束；主奏持续演奏':'按住触发，松开恢复'):item.id==='vibrato'?'咬嘴控制，或按乐曲速度在长音中渐入':'按键触发，或气息控制',mode:'off',strength:100,featured:true}));
}

function buildAdaptiveTechniquePlan(key = '', state = {}) {
  return mergeBackendTechniquePlan(key,state,state.techniqueMappings || techniqueMappings);
  const plan = buildTechniquePlan(key);
  const connected = !!state.deviceConnected;
  const hasButtons = !!state.hasAssignableButtons;
  const hasBite = !!state.hasBiteSensor;
  const hasThumb = !!state.hasThumbController;
  plan.forEach(item => {
    if (!connected && ['hardware','hybrid'].includes(item.mode)) {
      item.mode = 'auto';
      item.recommendation = '连接电吹管后自动选择最佳方式';
    } else if (connected && item.id === 'vibrato' && hasBite) {
      item.mode = 'hardware'; item.recommendation = '吹嘴咬合：连续控制深度';
    } else if (connected && item.id === 'growl' && (hasThumb || hasButtons)) {
      item.mode = 'hybrid'; item.recommendation = `${hasThumb ? '拇指控制器' : '功能键'}允许，气息控制深浅`;
    } else if (connected && ['flutter','mute','pizzicato','altFingering'].includes(item.id)) {
      if (hasButtons) {
        item.mode = 'hardware'; item.recommendation = '功能键：按下触发，松开恢复';
      } else if (['mute','pizzicato','altFingering'].includes(item.id)) {
        item.mode = 'off'; item.recommendation = '当前设备没有合适的独立控制器';
      } else {
        item.mode = 'breath'; item.recommendation = '气息动作：明显的快速强吹触发';
      }
    }
  });
  return plan;
}

function mergeBackendTechniquePlan(key, state, mappings) {
  const roles = buildTechniquePlan(key || currentInstrument?.instrumentKey || state.instrumentKey || '');
  if (!Array.isArray(mappings)) return roles;
  const modes = ['breath','breath','hardware','hardware','off'];
  return roles.map(item => {
    const backend = mappings.find(entry => entry.id === item.id);
    if (!backend) return {...item,mode:item.id==='vibrato'?'breath':'off',featured:true};
    return {
      ...item,
      index:Number(backend.technique ?? item.index),
      mode:modes[Number(backend.mode)] || item.mode,
      strength:Number.isFinite(Number(backend.strength)) ? Math.round(Number(backend.strength) * 100) : item.strength,
      recommendation:backend.recommendationReason || backend.recommendedSource || item.recommendation,
      featured:true,
      sourceType:Number(backend.sourceType || 0),
      sourceNumber:Number(backend.sourceNumber ?? -1),
      toggle:!!backend.toggle,
      pluginSupported:item.pluginSupported && backend.pluginSupported !== false,
      routeMissing:item.pluginSupported && backend.pluginSupported === false
    };
  });
}

function inferInstrumentKey(name = '') {
  const text = String(name).toLowerCase();
  const rules = [
    ['倍低音长号|double bass trombone','double-bass-trombone'],['次中低音长号|tenor bass trombone','tenor-bass-trombone'],
    ['低音长号|bass trombone','bass-trombone'],['中音长号|alto trombone','alto-trombone'],['长号|trombone','tenor-trombone'],
    ['降e调柔音号|flugelhorn.*eb','flugelhorn-eb'],['柔音号|flugelhorn','flugelhorn'],
    ['高音小号|piccolo trumpet','piccolo-trumpet'],['c调小号|trumpet.*\(c\)|trumpet c','trumpet-c'],['小号|trumpet','trumpet'],
    ['低音大号|bass tuba','bass-tuba'],['降e调大号|tuba.*eb','tuba-eb'],['上低音号|euphonium','euphonium'],
    ['降b调圆号|french horn.*bb','horn-bb'],['圆号|french horn|horn','horn-f'],
    ['高音萨克斯|soprano sax','soprano-sax'],['中音萨克斯|alto sax','alto-sax'],
    ['次中音萨克斯|tenor sax','tenor-sax'],['上低音萨克斯|baritone sax','baritone-sax'],
    ['低音长笛|bass flute','bass-flute'],['中音长笛|alto flute','alto-flute'],['短笛|piccolo','piccolo'],['长笛|flute','flute'],
    ['低音单簧管|bass clarinet','bass-clarinet'],['单簧管|clarinet','clarinet'],
    ['英国管|english horn|cor anglais','english-horn'],['双簧管|oboe','oboe'],
    ['倍低音巴松管|contrabassoon','contrabassoon'],['巴松管|bassoon','bassoon'],
    ['低音提琴重奏|double bass.*(?:section|ensemble)','double-bass-section'],
    ['小提琴重奏|violin.*(?:section|ensemble)','violin-section'],
    ['中提琴重奏|viola.*(?:section|ensemble)','viola-section'],
    ['大提琴重奏|cello.*(?:section|ensemble)','cello-section'],
    ['低音提琴|double bass','double-bass'],['小提琴|violin','violin'],['中提琴|viola','viola'],['大提琴|cello','cello']
  ];
  return rules.find(([pattern]) => new RegExp(pattern).test(text))?.[1] || 'alto-sax';
}

function isSupportedInstrument(item = {}) {
  if (item.supported === false) return false;
  return /swam|kong|qin|kontakt/i.test(`${item.name || ''} ${item.label || ''} ${item.brand || ''}`)
    || item.isSwam === true || item.brand === 'kong';
}

function showInstrumentArtwork(key, chineseName = '') {
  if(key==='kontakt-kensa')key='soprano-sax';
  const picture = $('#instrument-picture');
  if (key.startsWith('kong-') || key.startsWith('container:kong-v3:')) {
    currentArtworkKey = key;
    const name = String(chineseName || '').toLowerCase();
    const artwork = kongInstrumentArtwork[key]
      || (/二胡|高胡|中胡|马头琴/.test(name) ? 'instrument_kong_erhu.png'
        : /板胡|京胡/.test(name) ? 'instrument_kong_banhu.png'
        : /古筝/.test(name) ? 'instrument_kong_guzheng.png'
        : /古琴/.test(name) ? 'instrument_kong_guqin.png'
        : /琵琶/.test(name) ? 'instrument_kong_pipa.png'
        : /扬琴/.test(name) ? 'instrument_kong_yangqin.png'
        : /柳琴/.test(name) ? 'instrument_kong_liuqin.png'
        : /三弦/.test(name) ? 'instrument_kong_sanxian.png'
        : /埙/.test(name) ? 'instrument_kong_xun.png'
        : /中阮|阮/.test(name) ? 'instrument_kong_ruan.png'
        : /唢呐/.test(name) ? 'instrument_kong_suona.png'
        : /葫芦丝/.test(name) ? 'instrument_kong_hulusi.png'
        : /笙/.test(name) ? 'instrument_kong_sheng.png'
        : /箫/.test(name) ? 'instrument_kong_xiao.png'
        : /笛|巴乌|管子/.test(name) ? 'instrument_kong_dizi.png' : null);
    picture.hidden = !artwork;
    if (artwork) {
      picture.src = `../assets/instruments/${artwork}`;
      picture.alt = `当前乐器：${chineseName || '空音民乐'}`;
    } else picture.removeAttribute('src');
    $('#instrument-art').classList.remove('empty');
    $('#instrument-art').classList.toggle('folk', !artwork);
    $('#instrument-empty').querySelector('span').textContent = kongPresets.find(item => item[1] === key)?.[2] || chineseName?.slice(0,1) || '乐';
    $('#instrument-empty').querySelector('strong').textContent = chineseName;
    $('#instrument-empty').querySelector('small').textContent = '空音 Qin Engine V3 · 气息与技巧已适配';
    $('.breath-track').classList.remove('inactive');
    return;
  }
  const artwork = instrumentArtwork[key] || instrumentArtwork['alto-sax'];
  $('#instrument-art').classList.remove('folk');
  $('#instrument-empty').querySelector('span').textContent = '♫';
  $('#instrument-empty').querySelector('strong').textContent = '尚未加载乐器';
  $('#instrument-empty').querySelector('small').textContent = '选择音色方案后，这里才会显示对应乐器';
  picture.alt = `当前乐器：${chineseName || 'SWAM 乐器'}`;
  picture.hidden = false;
  $('#instrument-art').classList.remove('empty');
  $('.breath-track').classList.remove('inactive');
  if (currentArtworkKey === key) return;
  currentArtworkKey = key;
  picture.classList.add('changing');
  picture.src = `../assets/instruments/${artwork}`;
  window.setTimeout(() => picture.classList.remove('changing'), 130);
}

function clearInstrumentArtwork() {
  currentArtworkKey = '';
  const picture = $('#instrument-picture');
  picture.hidden = true;
  picture.removeAttribute('src');
  $('#instrument-art').classList.add('empty');
  $('#instrument-art').classList.remove('folk');
  $('.breath-track').classList.add('inactive');
  $('#breath-value').textContent = '—';
}

function applyPrototypeInstrument(key, chineseName, pluginName) {
  currentPluginLoaded = true;
  currentInstrument = {name:pluginName, chineseName, instrumentKey:key};
  const recommended = /qin|kong/i.test(pluginName) ? 3 : 1;
  renderBendRange({pluginLoaded:true,instrumentChineseName:chineseName,bendPreferenceKey:`preview:${key}`,
    bendRecommended:recommended,bendRange:recommended,bendRangeApplied:true});
  techniquePlan = buildAdaptiveTechniquePlan(key, latestBackendState);
  $('#sound-name').textContent = chineseName;
  $('#source-plugin-name').textContent = pluginName;
  showInstrumentArtwork(key, chineseName);
  renderSmartAdapter({deviceConnected:false,pluginLoaded:true,pluginName,instrumentChineseName:chineseName,instrumentKey:key});
  renderTechniqueMappings();
  renderInstrumentModelSwitcher(true);
  renderToneStyleSwitcher(true);
}

function nativeEvent(name, payload = {}) {
  const backend=window.__JUCE__?.backend;
  if (!backend?.emitEvent)return;
  backend.emitEvent(name,payload);
  // Coalesce explicit user changes; do not make viewport/clock/drag traffic
  // wake the entire plugin catalogue at the meter or mouse-move frequency.
  if(!meterOnlyNativeEvents.has(name)&&!backendRefreshTimer)
    backendRefreshTimer=setTimeout(()=>{backendRefreshTimer=0;backend.emitEvent('requestBackendState',{});},100);
}

function toast(message) {
  const el = $('#toast');
  el.textContent = message;
  el.classList.add('show');
  clearTimeout(el.timer);
  el.timer = setTimeout(() => el.classList.remove('show'), 2200);
}

function showPage(page) {
  if(page==='audio')page='settings';
  closeRecordingMenu();
  if(page!=='play')$('#record-success').hidden=true;
  if (compactMode) page = 'play';
  if (page === 'chain' && !studioUnlocked) return openStudioLogin();
  // Hide synchronously in the same bridge turn, not in a later animation
  // frame. This also works before a lost selection event has been recovered.
  if(page!=='play') {setNativeVideoExpanded(false);hideNativeVideoViewport();}
  if($('#bend-range-dialog'))$('#bend-range-dialog').hidden=true;
  $$('.nav-item,.page').forEach(el => el.classList.remove('active'));
  const button = $(`.nav-item[data-page="${page}"]`);
  button.classList.add('active');
  $(`#page-${page}`).classList.add('active');
  $('#page-title').textContent = titles[page];
  if (page === 'settings') nativeEvent('requestAudioSettings');
  if (page === 'sounds') promptForKongLibraryIfNeeded();
  updateVideoPresentation();
  requestVisualFrame();
}

function promptForKongLibraryIfNeeded() {
  if (kongLibraryPrompted || activeLibraryTab !== 'kong') return;
  const hasKong = availableInstruments.some(item => item.brand === 'kong' || /kong|qin/i.test(`${item.name || ''} ${item.label || ''}`));
  if (!hasKong || latestBackendState.scanning || latestBackendState.kongLibrary?.ready) return;
  kongLibraryPrompted = true;
  nativeEvent('chooseKongLibrary');
}

function updateTimeGreeting(now = new Date()) {
  const hour = now.getHours();
  const greeting = hour < 5 ? '夜深了' : hour < 11 ? '早上好' : hour < 14 ? '中午好' : hour < 18 ? '下午好' : '晚上好';
  $('#time-greeting').textContent = greeting;
}
updateTimeGreeting();
window.setInterval(updateTimeGreeting,60 * 1000);

$$('.nav-item').forEach(button => button.addEventListener('click', () => showPage(button.dataset.page)));
$('#choose-sound-from-empty').addEventListener('click', () => showPage('sounds'));

function escapeHtml(value) {
  return String(value ?? '').replace(/[&<>"']/g, character => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[character]));
}

function toneStylesForInstrument(key = '') {
 if (!key) return [{id:'none',name:'请先加载音色',eq:0,reverb:20,warmth:20}];
 return toneStyleLibrary[key] || [{id:'natural',name:'自然原声',description:'',eq:0,reverb:20,warmth:20}];
}

function toneVariantsForCurrentInstrument() {
  if (!currentInstrument) return toneStylesForInstrument('');
  const builtins = toneStylesForInstrument(currentInstrument.instrumentKey).map(style=>({...style,custom:false}));
  const customs = savedPresets.map((preset,index)=>({preset,index}))
    .filter(({preset})=>preset.brand==='kong' && !preset.studioDraft && preset.instrumentKey===currentInstrument.instrumentKey)
    .map(({preset,index})=>({id:`custom:${preset.id}`,name:preset.name,description:'我的音色方案',custom:true,presetIndex:index,
      eq:0,reverb:Math.round(Number(latestBackendState.reverbMix||.2)*100),warmth:20}));
  return [...customs,...builtins];
}

function applyToneStyle(style, announce = true) {
  if (!style) return;
  $('#tone-style-name').textContent = style.name;
  $('#tone-style-select').value = style.id;
  if ($('#chain-style-select')) $('#chain-style-select').value = style.id;
  if ($('#eq-tone')) {
    $('#eq-tone').value = String(style.eq);
    $('#eq-label').textContent = style.eq === 0 ? '自然' : `${style.eq > 0 ? '明亮度 +' : '温暖度 +'}${Math.abs(style.eq)}`;
  }
  if ($('#reverb-mix')) {
    $('#reverb-mix').value = String(style.reverb);
    $('#reverb-label').textContent = `强度 ${style.reverb}%`;
  }
  if ($('#tone-warmth')) {
    $('#tone-warmth').value = String(style.warmth);
    $('#tone-warmth-label').textContent = `${style.warmth}%`;
  }
  if ($('#performance-reverb')) {
    $('#performance-reverb').value = String(style.reverb);
    $('#performance-reverb-value').textContent = `${style.reverb}%`;
  }
  document.body.classList.remove('tone-style-changing');
  requestAnimationFrame(() => document.body.classList.add('tone-style-changing'));
  window.setTimeout(() => document.body.classList.remove('tone-style-changing'), 420);
  if (announce) {
    if (style.custom) {
      pendingPresetNavigation={kind:'preset',name:style.name};
      nativeEvent('loadPreset',{index:style.presetIndex});
      return toast(`正在载入：${style.name}`);
    }
    nativeEvent('setToneStyle', {id:style.id});
    toast(`已切换：${currentInstrument?.chineseName || '当前乐器'} · ${style.name}`);
  }
}

function renderToneStyleSwitcher(reset = false) {
  const enabled = !!currentPluginLoaded && !!currentInstrument;
  const key = currentInstrument?.instrumentKey || '';
  const styles = toneVariantsForCurrentInstrument();
  const instrumentChanged = currentToneStyleInstrument !== key;
  if (reset || instrumentChanged) {
    currentToneStyleInstrument = key;
    currentToneStyleIndex = 0;
  }
  currentToneStyleIndex = Math.max(0, Math.min(styles.length - 1, currentToneStyleIndex));
  const selector = $('#tone-style-select');
  // Meter snapshots must not destroy a live select (including an open native
  // popup on a touch device). Rebuild only when the actual catalogue changes.
  const options = styles.map(style => `<option value="${style.id}">${style.name}</option>`).join('');
  if (selector.dataset.optionsSignature !== options) {
    selector.innerHTML = options;
    selector.dataset.optionsSignature = options;
  }
  selector.disabled = !enabled;
  $('#tone-style-prev').disabled = !enabled || styles.length < 2;
  $('#tone-style-next').disabled = !enabled || styles.length < 2;
  $('#tone-style-switcher').classList.toggle('disabled', !enabled);
  if ($('#chain-style-select')) {
    if ($('#chain-style-select').dataset.optionsSignature !== options) {
      $('#chain-style-select').innerHTML = options;
      $('#chain-style-select').dataset.optionsSignature = options;
    }
    $('#chain-style-select').disabled = !enabled;
  }
  if (!enabled) {
    $('#tone-style-name').textContent = '尚未选择风格';
    return;
  }
  if (reset || instrumentChanged) applyToneStyle(styles[currentToneStyleIndex], false);
  else {
    selector.value = styles[currentToneStyleIndex]?.id || styles[0]?.id || '';
    if ($('#chain-style-select')) $('#chain-style-select').value = selector.value;
  }
}

function selectToneStyle(index) {
  if (!currentPluginLoaded || !currentInstrument) return toast('请先选择并加载一个音色方案');
  const styles = toneVariantsForCurrentInstrument();
  currentToneStyleIndex = (index + styles.length) % styles.length;
  applyToneStyle(styles[currentToneStyleIndex]);
}

$('#tone-style-prev').addEventListener('click', () => selectToneStyle(currentToneStyleIndex - 1));
$('#tone-style-next').addEventListener('click', () => selectToneStyle(currentToneStyleIndex + 1));
$('#tone-style-select').addEventListener('change', event => {
  const styles = toneVariantsForCurrentInstrument();
  selectToneStyle(Math.max(0, styles.findIndex(style => style.id === event.target.value)));
});

function instrumentModelsForCurrentInstrument() {
  if (!currentInstrument) return [];
  const supplied = latestBackendState?.instrumentModels;
  if (Array.isArray(supplied) && supplied.length) return supplied.map((item,index) => ({id:String(item.id ?? index),name:String(item.name || item.label || `型号 ${index + 1}`)}));
  if (window.__JUCE__?.backend?.emitEvent) return [];
  const pluginName = String(currentInstrument.name || currentInstrument.chineseName || '当前乐器');
  if (/sax/i.test(pluginName)) {
    const familyName = String(currentInstrument.chineseName || '萨克斯');
    return ['A','B','C'].map((suffix,index) => ({id:String(index),name:`${familyName} ${suffix}`}));
  }
  return [{id:'default',name:latestBackendState?.instrumentModelName || pluginName}];
}

function renderInstrumentModelSwitcher(reset = false) {
  const models = instrumentModelsForCurrentInstrument();
  const key = currentInstrument?.instrumentKey || '';
  const isKong = latestBackendState?.pluginBrand === 'kong' || currentInstrument?.brand === 'kong'
    || key.startsWith('kong-') || key.startsWith('container:kong-v3:');
  const modelSwitcher = $('#instrument-model-switcher');
  modelSwitcher.hidden = true;
  const enabled = false;
  const backendName = latestBackendState?.instrumentModelName;
  const backendIndex = models.findIndex(model => model.name === backendName || model.id === String(latestBackendState?.instrumentModelId ?? ''));
  if (reset || currentInstrumentModelKey !== key) {
    currentInstrumentModelKey = key;
    currentInstrumentModelIndex = backendIndex >= 0 ? backendIndex : Math.max(0, models.findIndex(model => model.name === currentInstrument?.name));
  }
  else if (backendIndex >= 0) currentInstrumentModelIndex = backendIndex;
  currentInstrumentModelIndex = Math.max(0, Math.min(models.length - 1, currentInstrumentModelIndex));
  const selector = $('#instrument-model-select');
  selector.innerHTML = models.map(model => `<option value="${escapeHtml(model.id)}">${escapeHtml(model.name)}</option>`).join('');
  selector.disabled = !enabled;
  if (enabled) selector.value = models[currentInstrumentModelIndex].id;
  $('#instrument-model-name').textContent = enabled ? models[currentInstrumentModelIndex].name
    : currentPluginLoaded ? '当前音源未开放型号' : '尚未加载乐器';
  $('#instrument-model-prev').disabled = !enabled || models.length < 2;
  $('#instrument-model-next').disabled = !enabled || models.length < 2;
  modelSwitcher.classList.toggle('disabled', !enabled);
}

function selectInstrumentModel(index) {
  const models = instrumentModelsForCurrentInstrument();
  if (!currentPluginLoaded || models.length < 2) return;
  currentInstrumentModelIndex = (index + models.length) % models.length;
  const model = models[currentInstrumentModelIndex];
  $('#instrument-model-name').textContent = model.name;
  nativeEvent('setInstrumentModel', {id:model.id,name:model.name,index:currentInstrumentModelIndex});
  toast(`已切换乐器：${model.name}`);
}

$('#instrument-model-prev').addEventListener('click', () => selectInstrumentModel(currentInstrumentModelIndex - 1));
$('#instrument-model-next').addEventListener('click', () => selectInstrumentModel(currentInstrumentModelIndex + 1));
$('#instrument-model-select').addEventListener('change', event => {
  const models = instrumentModelsForCurrentInstrument();
  selectInstrumentModel(Math.max(0,models.findIndex(model => model.id === event.target.value)));
});
$('#chain-style-select')?.addEventListener('change', event => {
  const styles = toneStylesForInstrument(currentInstrument?.instrumentKey);
  selectToneStyle(Math.max(0, styles.findIndex(style => style.id === event.target.value)));
});
$('#tone-warmth')?.addEventListener('input', event => {
  $('#tone-warmth-label').textContent = `${event.target.value}%`;
  updateBuiltinEffects();
});
const expertControls = [
  ['bass','低频 EQ · 180 Hz / ±12 dB',50],['brightness','高频色彩 · EQ',50],
  ['air','空气感 · Air',0],
  ['warmth','温暖度 · Warmth',20],['saturation','饱和度 · Saturation',12],
  ['compression','压缩阈值 · Threshold',35],['ratio','压缩比 · Ratio',25],
  ['harsh','去刺耳 · Harsh Control',25],['reverb','混响强度 · Reverb Mix',18],
  ['room','空间大小 · Room Size',42],['damping','高频阻尼 · Damping',54],
  ['width','立体声宽度 · Width',88],['output','输出音量 · Output Gain',50]
];
function toneStyleToExpert() {
  const style = toneStylesForInstrument(currentInstrument?.instrumentKey)[currentToneStyleIndex] || {};
  if (latestBackendState?.toneSettings && window.__JUCE__?.backend?.emitEvent)
    return Object.fromEntries(expertControls.map(([id,,fallback]) => [id,Number(latestBackendState.toneSettings[id] ?? fallback)]));
  return {bass:50,air:0,brightness:Math.round((Number(style.eq || 0)+100)/2),warmth:Number(style.warmth ?? 20),compression:35,harsh:25,reverb:Number(style.reverb ?? 18),room:42,output:50};
}
function renderExpertControls() {
  $('#expert-grid').innerHTML = expertControls.map(([id,label,fallback]) => `<div class="expert-control"><label for="expert-${id}"><span>${label}</span><b id="expert-${id}-value">${expertDisplay(id,expertSettings?.[id] ?? fallback)}</b></label><input id="expert-${id}" data-expert="${id}" type="range" min="0" max="100" step="0.001" value="${expertSettings?.[id] ?? fallback}" ${currentPluginLoaded ? '' : 'disabled'}></div>`).join('');
  $$('[data-expert]').forEach(input => input.addEventListener('input', () => {
    expertSettings[input.dataset.expert] = Number(input.value);
    $(`#expert-${input.dataset.expert}-value`).textContent = expertDisplay(input.dataset.expert,Number(input.value));
    expertDirty = true;
    nativeEvent('previewCustomTone', expertSettings);
  }));
}
function closeExpert(restore = true) {
  if (restore && expertDirty) nativeEvent('cancelCustomTone');
  $('#expert-dialog').hidden = true;
  expertDirty = false;
}
$('#tone-expert')?.addEventListener('click', () => {
  if (!currentPluginLoaded) return toast('请先加载一个乐器音源');
  showPage('chain');
  refreshInlineExpert();
});
function expertDisplay(id,value) {
  if (id === 'bass') return `${((value/50-1)*12).toFixed(1)} dB`;
  if (id === 'ratio') return `${(1+value/100*3).toFixed(2)}:1`;
  if (id === 'compression') return `${(20*Math.log10(.85-value/100*.55)).toFixed(1)} dB`;
  if (id === 'output') return value<=0?'−∞ dB':`${(20*Math.log10(value/50)).toFixed(1)} dB`;
  return `${value.toFixed(1)}%`;
}
// The custom-tone editor lives directly in the second step.
$('#custom-effects-step').append(document.querySelector('#expert-dialog .expert-inline'));
function enterCustomToneCreate() {
  if (!studioUnlocked) return;
  studioSavedId='';
  if ($('#studio-instrument-name')) $('#studio-instrument-name').value='';
  if ($('#studio-style-name')) $('#studio-style-name').value='';
  nativeEvent('beginCustomTone');
  customSourceReady=false;
  customToneMode='create'; editingPresetName=''; expertDirty=false;
  $('#custom-tone-empty').hidden=true; $('#custom-tone-editor').hidden=false; $('#custom-effects-step').hidden=true;
  $('#instrument-select').disabled=false; $('#instrument-select').value=''; $('#open-instrument').disabled=true;
  $('#instrument-load-state').textContent='请选择一个乐器音源';
  $('#save-expert').textContent='保存草稿'; $('#save-as-expert').hidden=true; $('#reset-expert').hidden=true;
  $('#cancel-expert').textContent='取消定制';
}
function enterCustomToneEdit(name) {
  customSourceReady=true;
  customToneMode='edit'; editingPresetName=name; expertDirty=false;
  $('#custom-tone-empty').hidden=true; $('#custom-tone-editor').hidden=false; $('#custom-effects-step').hidden=false;
  $('#instrument-select').disabled=true; $('#open-instrument').disabled=false;
  $('#save-expert').textContent='保存草稿'; $('#save-as-expert').hidden=true; $('#reset-expert').hidden=true;
  $('#cancel-expert').textContent='取消修改'; refreshInlineExpert();
}
$('#start-custom-tone').addEventListener('click', enterCustomToneCreate);
function refreshInlineExpert() {
  if (customToneMode === 'idle' || !customSourceReady) return;
  if (expertDirty || $('#expert-grid').contains(document.activeElement)) return;
  expertSettings = toneStyleToExpert();
  renderExpertControls();
  $('#save-expert').textContent = '保存草稿';
  $('#save-expert').disabled = !currentPluginLoaded;
  $('#save-as-expert').disabled = !currentPluginLoaded;
}
$('#save-as-expert').addEventListener('click', () => {
  editingPresetName = '';
  nativeEvent('cancelPresetEdit');
  $('#preset-name-input').value = '';
  $('#preset-name-dialog').hidden = false;
  $('#preset-name-input').focus();
});
refreshInlineExpert();
$('#cancel-expert-open')?.addEventListener('click', () => { $('#expert-confirm-dialog').hidden = true; });
$('#confirm-expert-open')?.addEventListener('click', () => {
  $('#expert-confirm-dialog').hidden = true;
  expertSettings = toneStyleToExpert();
  expertDirty = false;
  $('#expert-base-style').textContent = `基于：${toneStylesForInstrument(currentInstrument?.instrumentKey)[currentToneStyleIndex]?.name || '自然原声'}`;
  renderExpertControls();
  $('#save-expert').textContent = editingPresetName ? '保存修改' : '保存为我的方案';
  showPage('chain');
});
$('#reset-expert')?.addEventListener('click', () => {
  expertSettings = toneStyleToExpert(); expertDirty = false; renderExpertControls(); nativeEvent('cancelCustomTone');
});
$('#cancel-expert')?.addEventListener('click', () => {
  if (expertDirty) nativeEvent('cancelCustomTone');
  nativeEvent('cancelPresetEdit');
  customToneMode='idle'; editingPresetName=''; expertDirty=false;
  $('#custom-tone-editor').hidden=true; $('#custom-tone-empty').hidden=false; showPage('sounds');
});
$('#save-expert')?.addEventListener('click', () => {
  saveStudioDraft(false);
});
$('#cancel-preset-name')?.addEventListener('click', () => { $('#preset-name-dialog').hidden = true; });
$('#confirm-preset-name')?.addEventListener('click', () => {
  const name = $('#preset-name-input').value.trim();
  if (!name) return toast('请输入方案名称');
  const duplicate = savedPresets.some(item => String(item.name || '').toLowerCase() === name.toLowerCase()
    && (item.brand || 'swam') === ((currentInstrument?.brand === 'kong'
      || currentInstrument?.instrumentKey?.startsWith('kong-')
      || currentInstrument?.instrumentKey?.startsWith('container:kong-v3:')) ? 'kong' : 'swam'));
  if (duplicate && !window.confirm(`已有名为“${name}”的方案。\n\n点击“确定”覆盖，点击“取消”返回改名。`)) return;
  nativeEvent('saveCustomPreset', {name,settings:expertSettings,baseStyleId:toneStylesForInstrument(currentInstrument?.instrumentKey)[currentToneStyleIndex]?.id || 'natural'});
  if (!window.__JUCE__?.backend?.emitEvent) {
    savedPresets.unshift({name,brand:(currentInstrument?.brand === 'kong'
      || currentInstrument?.instrumentKey?.startsWith('kong-')
      || currentInstrument?.instrumentKey?.startsWith('container:kong-v3:'))?'kong':'swam',instrumentChineseName:currentInstrument?.chineseName});
    renderPresets();
  }
  $('#preset-name-dialog').hidden = true;
  expertDirty=false; customToneMode='idle'; showPage('sounds');
  toast(`已保存：${name}`);
});
renderToneStyleSwitcher();

// Use the actual available content width, including Windows display scaling.
new ResizeObserver(entries => {
  const width = entries[0].contentRect.width;
  const columns = Math.max(1, Math.min(5, Math.floor((width + 9) / 170)));
  entries[0].target.style.setProperty('--preset-columns', columns);
  entries[0].target.style.setProperty('--scan-span', Math.max(1, columns - 1));
}).observe($('#preset-grid'));

function bindInstrumentSorting() {
  const grid = $('#preset-grid');
  const original = [...grid.querySelectorAll('.instrument-preset-card')];
  let source = null, committed = false, frame = 0, pointerX = 0, pointerY = 0, lastMove = 0;
  let scrollHost = document.scrollingElement;
  for (let parent = grid; parent; parent = parent.parentElement) {
    if (/(auto|scroll)/.test(getComputedStyle(parent).overflowY) && parent.scrollHeight > parent.clientHeight) {
      scrollHost = parent; break;
    }
  }
  function preview(target) {
    if (!source || !target || source === target || performance.now() - lastMove < 160) return;
    const nodes = [...grid.querySelectorAll('.instrument-preset-card')];
    const rects = new Map(nodes.map(node => [node, node.getBoundingClientRect()]));
    const forward = nodes.indexOf(source) < nodes.indexOf(target);
    grid.insertBefore(source, forward ? target.nextSibling : target);
    lastMove = performance.now();
    if (!matchMedia('(prefers-reduced-motion: reduce)').matches) nodes.forEach(node => {
      if (node === source) return;
      const before = rects.get(node), after = node.getBoundingClientRect();
      node.getAnimations().forEach(animation => animation.cancel());
      node.animate([{transform:`translate(${before.left-after.left}px,${before.top-after.top}px)`},
        {transform:'translate(0,0)'}], {duration:180,easing:'ease-out'});
    });
  }
  function tick() {
    if (!source) return;
    const rect = scrollHost === document.scrollingElement
      ? {top:0,bottom:innerHeight} : scrollHost.getBoundingClientRect();
    const bounds = {top:Math.max(0,rect.top),bottom:Math.min(innerHeight,rect.bottom)};
    const edge = 85;
    const delta = pointerY < bounds.top + edge ? -Math.min(16,(bounds.top+edge-pointerY)/5)
      : pointerY > bounds.bottom-edge ? Math.min(16,(pointerY-bounds.bottom+edge)/5) : 0;
    if (delta) {
      scrollHost.scrollTop += delta;
      preview(document.elementFromPoint(pointerX,pointerY)?.closest('.instrument-preset-card'));
    }
    frame = requestAnimationFrame(tick);
  }
  function cleanup() {
    lastToneDragAt=Date.now();
    cancelAnimationFrame(frame);
    document.removeEventListener('dragover', track);
    if (!committed) original.forEach(node => grid.appendChild(node));
    source?.classList.remove('dragging'); source = null; draggedInstrumentCardKey = '';
    renderPresets();
  }
  function track(event) { pointerX=event.clientX; pointerY=event.clientY; }
  original.forEach(card => {
    card.addEventListener('dragstart', event => {
      if (event.target.closest('button,select,input')) return event.preventDefault();
      source=card; committed=false; draggedInstrumentCardKey=card.dataset.cardKey;
      card.classList.add('dragging'); track(event);
      event.dataTransfer.effectAllowed='move'; event.dataTransfer.setData('text/plain',draggedInstrumentCardKey);
      document.addEventListener('dragover',track); frame=requestAnimationFrame(tick);
    });
    card.addEventListener('dragover', event => {
      if (!source) return;
      event.preventDefault(); event.dataTransfer.dropEffect='move'; track(event); preview(card);
    });
    card.addEventListener('dragend',cleanup);
  });
  grid.ondragover = event => { if (source) {event.preventDefault();track(event);} };
  grid.ondrop = event => {
    if (!source) return;
    event.preventDefault(); committed=true;
    const order=[...grid.querySelectorAll('.instrument-preset-card')].map(card=>card.dataset.cardKey);
    try { localStorage.setItem(`fengyin-card-order-${activeLibraryTab}`,JSON.stringify(order));
      nativeEvent('reorderInstrumentCards',{library:activeLibraryTab,order});
      toast('乐器顺序已保存');
    } catch { committed=false; toast('顺序保存失败，请重试'); }
    cleanup();
  };
}

function renderPresets() {
  if (draggedInstrumentCardKey) return;
  $('#library-tabs').hidden=true; $('#library-context').hidden=true;
  const cards=[];
  for (const tone of publishedTones) {
    const pluginIndex=availableInstruments.findIndex(p=>tone.kind==='kong' ? p.brand==='kong' : tone.kind==='kontakt' ? p.brand==='kontakt' : p.instrumentKey===tone.instrumentKey);
    cards.push({id:`builtin:${tone.id}`,catalogId:tone.id,name:tone.name,
      pluginIndex,styleId:tone.styleId,instrumentKey:tone.instrumentKey,kind:tone.kind,index:-1,missing:pluginIndex<0});
  }
  savedPresets.forEach((p,index)=>{
    if(p.studioDraft || !p.published) return;
    cards.push({id:`custom:${p.id}`,presetId:p.id,name:p.name,index,
      missing:!availableInstruments.some(i=>i.pluginId===p.pluginId)});
  });
  let order=[]; try { order=JSON.parse(localStorage.getItem('fengyin-card-order-swam')||'[]'); } catch {}
  // Migrate old instrument/style keys, including both formerly-colliding violins.
  order=order.flatMap(key=>{
    if(key==='builtin:violin')return ['builtin:violin-bright'];
    if(cards.some(c=>c.id===key))return [key];
    return publishedTones.filter(t=>`builtin:${t.instrumentKey}:${t.styleId}`===key).map(t=>`builtin:${t.id}`);
  });
  cards.sort((a,b)=>(order.includes(a.id)?order.indexOf(a.id):1e9)-(order.includes(b.id)?order.indexOf(b.id):1e9));
  $('#preset-grid').innerHTML=cards.map(c=>`<section class="instrument-preset-card compact ${latestBackendState.activeToneVariantId===c.id?'active':''}" data-card-key="${escapeHtml(c.id)}" tabindex="0" role="button">
    <h3>${escapeHtml(c.name)}</h3><span class="card-drag-handle" draggable="true" aria-label="拖动排序">⠿</span>
    ${c.index>=0?'<button class="delete-tone" type="button">删除</button>':''}
    ${c.missing?'<small>缺少音源</small>':toneLoadInFlight&&pendingPresetNavigation?.name===c.name?'<small>正在加载…</small>':''}</section>`).join('');
  function load(c) {
    if(c.missing)return toast('缺少此方案所需音源，请先安装对应音源');
    if(toneLoadInFlight){queuedToneLoad=()=>load(c);return;}
    if(latestBackendState.pluginLoading||latestBackendState.effectLoading)return toast('正在加载，请稍候');
    toneLoadInFlight=true;
    pendingPresetNavigation=c.index>=0?{kind:'preset',name:c.name}
      :{kind:c.kind==='swam'?'plugin':'published',name:c.name,styleId:c.styleId};
    if(c.index>=0) nativeEvent('loadPreset',{index:c.index});
    else if(c.kind==='swam') nativeEvent('loadPlugin',{index:c.pluginIndex});
    else nativeEvent('loadPublishedTone',{id:c.catalogId});
    toast(`正在载入：${c.name}`);
    renderPresets();
  }
  $$('#preset-grid .instrument-preset-card').forEach((el,i)=>{
    el.onclick=e=>{if(Date.now()-lastToneDragAt<300||e.target.closest('.card-drag-handle'))return;
      if(e.target.closest('.delete-tone')){nativeEvent('deletePreset',{id:cards[i].presetId});return;} load(cards[i]);};
    el.onkeydown=e=>{if(e.target===el&&(e.key==='Enter'||e.key===' ')){e.preventDefault();load(cards[i]);}};
  });
  bindInstrumentSorting();
  let quick=$('#quick-tones');
  if(!quick){quick=document.createElement('div');quick.id='quick-tones';document.body.appendChild(quick);}
  quick.innerHTML=cards.map((c,i)=>`<button class="${latestBackendState.activeToneVariantId===c.id?'active':''}" data-tone="${i}">${escapeHtml(c.name)}${c.missing?' · 缺少音源':''}</button>`).join('');
  quick.querySelectorAll('button').forEach(b=>b.onclick=()=>load(cards[Number(b.dataset.tone)]));
  const compactList = $('#compact-tones');
  if (compactList) {
    compactList.innerHTML = '<h3>音色方案</h3>' + cards.map((c,i)=>`<button type="button" title="${escapeHtml(c.name)}" class="${latestBackendState.activeToneVariantId===c.id?'active':''}" aria-pressed="${latestBackendState.activeToneVariantId===c.id}" data-tone="${i}"><span>${escapeHtml(c.name)}</span>${c.missing?'<small>缺少音源</small>':''}</button>`).join('');
    compactList.querySelectorAll('button').forEach(b=>{
      b.onclick=()=>load(cards[Number(b.dataset.tone)]);
      const label=b.querySelector('span');
      const startScroll=()=>{
        label.classList.remove('scrolling');
        label.style.removeProperty('--tone-scroll-distance');
        label.style.removeProperty('--tone-scroll-duration');
        const distance=Math.max(0,label.scrollWidth-label.clientWidth);
        if(distance<2) return;
        label.style.setProperty('--tone-scroll-distance',`${distance}px`);
        label.style.setProperty('--tone-scroll-duration',`${Math.max(2.4,distance/24).toFixed(2)}s`);
        label.classList.add('scrolling');
      };
      const stopScroll=()=>label.classList.remove('scrolling');
      b.addEventListener('mouseenter',startScroll);
      b.addEventListener('mouseleave',stopScroll);
      b.addEventListener('focus',startScroll);
      b.addEventListener('blur',stopScroll);
    });
  }
}
renderPresets();
let quickHoverTimer;
let quickReopenBlocked = false;
function closeQuickTones() {
  clearTimeout(quickHoverTimer);
  $('#quick-tones')?.classList.remove('open');
  document.body.classList.remove('quick-tones-open');
  quickReopenBlocked = !!document.querySelector('.nav-item[data-page="sounds"]:hover');
}
{
  const entry=document.querySelector('.nav-item[data-page="sounds"]');
  const backdrop=document.createElement('div');
  // Keep the backdrop in the same stacking context as the sidebar. A body-level
  // overlay otherwise covers the entire z-index:1 app shell, including the
  // supposedly higher sidebar, and synthesizes repeated leave/enter events.
  backdrop.id='quick-tones-backdrop';$('.app-shell').appendChild(backdrop);
  backdrop.onclick=closeQuickTones;
  const hide=()=>{clearTimeout(quickHoverTimer);quickHoverTimer=setTimeout(()=>{
    if(entry.matches(':hover') || $('#quick-tones')?.matches(':hover'))return;
    closeQuickTones();
  },250);};
  entry.addEventListener('mouseenter',()=>{
    clearTimeout(quickHoverTimer);
    if(quickReopenBlocked || compactMode)return;
    clearTimeout(quickHoverTimer);quickHoverTimer=setTimeout(()=>{
      const panel=$('#quick-tones');if(!panel)return;
      const rect=entry.getBoundingClientRect();
      panel.style.left=`${rect.right+8}px`;panel.style.top='16px';
      panel.style.transformOrigin=`left ${rect.top+rect.height/2-16}px`;
      panel.classList.add('open');document.body.classList.add('quick-tones-open');
      panel.onmouseenter=()=>clearTimeout(quickHoverTimer);panel.onmouseleave=hide;
    },180);
  });
  entry.addEventListener('mouseleave',()=>{quickReopenBlocked=false;hide();});
  entry.addEventListener('click',closeQuickTones);
  document.addEventListener('keydown',e=>{
    if(e.key==='Escape' && $('#quick-tones')?.classList.contains('open')){
      e.stopImmediatePropagation();closeQuickTones();
    }
  },true);
}

function updateLibraryContext() {
  const isKong = activeLibraryTab === 'kong';
  $('#library-context-title').textContent = isKong ? '已识别空音 Qin Engine V3' : '已识别 SWAM 音源';
  $('#library-context-status').textContent = '本地音源';
}

$$('.library-tab').forEach(button => button.addEventListener('click', () => {
  activeLibraryTab = button.dataset.library;
  $$('.library-tab').forEach(tab => {
    const selected = tab === button;
    tab.classList.toggle('active', selected);
    tab.setAttribute('aria-selected', String(selected));
  });
  updateLibraryContext();
  renderPresets();
  promptForKongLibraryIfNeeded();
}));
updateLibraryContext();
const previewParams = new URLSearchParams(window.location.search);
if (previewParams.get('library') === 'kong') $('.library-tab[data-library="kong"]')?.click();
if (previewParams.get('preview') === 'sounds') showPage('sounds');

const video = $('#video');
const videoFileInput = $('#video-file');
function updateVideoPresentation() {
  if(nativeVideoEngine){scheduleNativeVideoViewport();return;}
  const element=document.querySelector('#video'),card=document.querySelector('.video-card');
  if(!element || !nativeAccompanimentReady || !videoWantsPlaying)return;
  const visible=!document.hidden && !document.body.classList.contains('presentation-hidden')
    && document.querySelector('#page-play')?.classList.contains('active') && card?.getClientRects().length>0;
  if(!visible) element.pause(); // Audio continues in the native transport.
  else nativeEvent('syncVideoPlayback',{generation:videoGeneration,position:element.currentTime});
}
function chooseVideoFile() {
  videoWantsPlaying = false;
  video.pause();
  if(nativeVideoEngine)nativeEvent('setVideoPlaybackState',{generation:videoGeneration,playing:false});
  if (window.__JUCE__?.backend?.emitEvent) {
    nativeEvent('chooseVideo');
    toast('请选择一个伴奏视频');
  } else {
    videoFileInput.click();
  }
}
$('#choose-video').addEventListener('click', chooseVideoFile);
$('#change-video').addEventListener('click', chooseVideoFile);
videoFileInput.addEventListener('change', event => {
  const file = event.target.files[0];
  if (!file) return;
  selectedVideoFile = file;
  videoLoading = true;
  videoWantsPlaying = false;
  ++videoPlayRequest;
  videoGeneration += 1;
  videoSeekRevision=0;
  nativeVideoFinished=false;
  preparedVideoSource=null;restoringVideoPosition=null;usingPreparedVideo=false;
  lastVideoSyncAt = 0;
  videoSeekDragging=false;pendingVideoSeek=null;
  $('#seek').value = 0;
  $('#current-time').textContent = '00:00';
  $('#duration').textContent = '00:00';
  nativeAccompanimentReady = false;
  video.muted = false;
  triedVideoDataFallback = false;
  video.pause();
  video.style.display = 'none';
  $('#video-empty').style.display = 'grid';
  if (videoUrl) URL.revokeObjectURL(videoUrl);
  videoUrl = URL.createObjectURL(file);
  originalVideoSource=videoUrl;
  video.src = videoUrl;
  video.load();
  $('#video-name').textContent = file.name;
  $('#change-video').textContent = '更换视频';
  event.target.value = '';
  toast('正在载入视频…');
});
video.addEventListener('loadedmetadata', () => {
  if(nativeVideoEngine)return;
  videoLoading = false;
  if(restoringVideoPosition!==null)video.currentTime=restoringVideoPosition;
  else video.currentTime = 0;
  restoringVideoPosition=null;
  video.style.display = 'block';
  $('#video-empty').style.display = 'none';
  $('#duration').textContent = formatTime(video.duration);
  toast('视频已载入，可以播放');
});
video.addEventListener('error', () => {
  if(nativeVideoEngine)return;
  if(usingPreparedVideo && originalVideoSource) {
    usingPreparedVideo=false;restoringVideoPosition=restoringVideoPosition??(Number.isFinite(video.currentTime)?video.currentTime:0);
    videoLoading=true;video.src=originalVideoSource;video.load();return;
  }
  if (!triedVideoDataFallback && selectedVideoFile && selectedVideoFile.size <= 300 * 1024 * 1024) {
    triedVideoDataFallback = true;
    const reader = new FileReader();
    reader.onload = () => { video.src = reader.result; video.load(); };
    reader.onerror = () => toast('无法读取这个视频文件，请确认文件没有损坏');
    reader.readAsDataURL(selectedVideoFile);
    return;
  }
  const reason = video.error?.message || `浏览器错误代码 ${video.error?.code || '未知'}`;
  toast(`视频无法播放：${reason}`);
  window.fengyinVoice?.say('V037');
  $('#video-name').textContent = '视频编码不受支持，请转换为 H.264 + AAC';
});

function toggleVideo() {
  if(nativeVideoEngine) {
    if(!originalVideoSource)return toast('请先选择一个本地视频');
    if(!nativeAccompanimentReady)return toast('视频正在准备，请稍候');
    videoWantsPlaying=!videoWantsPlaying;
    if(videoWantsPlaying && nativeVideoFinished) {
      nativeVideoFinished=false;nativeVideoPosition=0;
      nativeEvent('seekVideo',{generation:videoGeneration,position:0,seekRevision:++videoSeekRevision});
    }
    nativeEvent('setVideoPlaybackState',{generation:videoGeneration,playing:videoWantsPlaying});
    $('#play-button').textContent=videoWantsPlaying?'Ⅱ':'▶';
    return;
  }
  if (!video.src) { window.fengyinVoice?.say('V038'); return toast('请先选择一个本地视频'); }
  if (videoLoading || (window.__JUCE__?.backend?.emitEvent && !nativeAccompanimentReady))
    return toast('视频正在准备，请稍候');
  videoWantsPlaying = !videoWantsPlaying;
  const request = ++videoPlayRequest;
  if (!videoWantsPlaying) {
    video.pause();
    nativeEvent('setVideoPlaybackState',{generation:videoGeneration,playing:false,position:video.currentTime});
    applyPreparedVideoSource();
  } else {
    if(nativeAccompanimentReady && (nativeVideoFinished || video.ended)) {
      nativeVideoFinished=false;video.currentTime=0;
      nativeEvent('seekVideo',{generation:videoGeneration,position:0,seekRevision:++videoSeekRevision});
    }
    video.play().catch(error => {
    if (request !== videoPlayRequest) return;
    videoWantsPlaying = false;
    if (error.name !== 'AbortError') toast('暂时无法播放，请重新选择视频');
  });
  }
}
$('#play-button').addEventListener('click', toggleVideo);
video.addEventListener('playing', () => { if(nativeVideoEngine)return; if (videoLoading || !videoWantsPlaying) { video.pause(); return; } document.body.classList.add('video-playing'); nativeEvent('setVideoPlaybackState',{generation:videoGeneration,playing:true,position:video.currentTime}); $('#play-button').textContent='Ⅱ';updateVideoPresentation(); });
video.addEventListener('pause', () => { if(nativeVideoEngine)return; document.body.classList.remove('video-playing'); if (!videoLoading && (!nativeAccompanimentReady || !videoWantsPlaying)) nativeEvent('setVideoPlaybackState',{generation:videoGeneration,playing:false,position:video.currentTime}); $('#play-button').textContent=videoWantsPlaying?'Ⅱ':'▶'; });
video.addEventListener('ended', () => {
  if(nativeVideoEngine)return;
  document.body.classList.remove('video-playing');
  if(nativeAccompanimentReady)return; // The picture must not truncate the audio tail.
  videoWantsPlaying = false;nativeEvent('setVideoPlaybackState',{generation:videoGeneration,playing:false,position:video.currentTime});
});
video.addEventListener('timeupdate', () => {
  if(nativeVideoEngine)return;
  $('#current-time').textContent = formatTime(video.currentTime);
  if(!videoSeekDragging) $('#seek').value = video.duration ? video.currentTime / video.duration * 100 : 0;
  const now = performance.now();
  if(latestBackendState.audioTimingEnabled && videoWantsPlaying && !video.paused && video.getVideoPlaybackQuality) {
    const quality=video.getVideoPlaybackQuality();
    if(videoMetricsBaseline?.generation!==videoGeneration)videoMetricsBaseline={generation:videoGeneration,time:now,frames:quality.totalVideoFrames,dropped:quality.droppedVideoFrames};
    if(now-videoMetricsBaseline.time>=5000) {
      const frames=quality.totalVideoFrames-videoMetricsBaseline.frames,dropped=quality.droppedVideoFrames-videoMetricsBaseline.dropped;
      nativeEvent('videoPlaybackMetrics',{generation:videoGeneration,fps:Math.max(0,frames-dropped)*1000/(now-videoMetricsBaseline.time),dropped:Math.max(0,dropped),width:video.videoWidth,height:video.videoHeight,proxy:usingPreparedVideo,clockCorrections:videoClockCorrections});
      videoMetricsBaseline={generation:videoGeneration,time:now,frames:quality.totalVideoFrames,dropped:quality.droppedVideoFrames};
    }
  } else videoMetricsBaseline=null;
  if (!videoLoading && !videoSeekDragging && videoWantsPlaying && nativeAccompanimentReady && now-lastVideoSyncAt > 750) {
    lastVideoSyncAt = now;
    nativeEvent('syncVideoPlayback',{generation:videoGeneration,position:video.currentTime});
  }
});
function applyPreparedVideoSource() {
  if(nativeVideoEngine)return;
  if(!preparedVideoSource || videoWantsPlaying || videoLoading || videoSeekDragging
      || recording || screenRecordingBusy || Number(latestBackendState?.breath||0)>.01) return;
  const source=preparedVideoSource;preparedVideoSource=null;
  if(Number(source.generation)!==videoGeneration)return;
  restoringVideoPosition=Number.isFinite(video.currentTime)?video.currentTime:0;
  usingPreparedVideo=true;videoLoading=true;video.src=source.url;video.load();
}
window.__JUCE__?.backend?.addEventListener('videoPlaybackPrepared',source=>{
  if(Number(source?.generation)!==videoGeneration || !source.url || !nativeAccompanimentReady)return;
  preparedVideoSource=source;applyPreparedVideoSource();
});
$('#seek').addEventListener('input', event => {
  const duration=nativeVideoEngine?nativeVideoDuration:video.duration;
  if(!duration) return;
  pendingVideoSeek = event.target.value / 100 * duration;
  $('#current-time').textContent = formatTime(pendingVideoSeek);
});
function commitVideoSeek() {
  videoSeekDragging = false;
  if(pendingVideoSeek === null || !Number.isFinite(pendingVideoSeek)) return;
  const position=pendingVideoSeek;pendingVideoSeek=null;
  nativeVideoFinished=false;
  if(nativeVideoEngine)nativeVideoPosition=position;
  else video.currentTime=position;
  lastVideoSyncAt=performance.now();
  nativeEvent('seekVideo',{generation:videoGeneration,position,seekRevision:++videoSeekRevision});
}
$('#seek').addEventListener('pointerdown',()=>{videoSeekDragging=true;});
$('#seek').addEventListener('change',commitVideoSeek);
$('#seek').addEventListener('pointerup',commitVideoSeek);
$('#seek').addEventListener('pointercancel',()=>{videoSeekDragging=false;pendingVideoSeek=null;});
function applyVideoPlaybackClock(clock) {
  if(nativeVideoEngine)return;
  if(Number(clock?.generation)!==videoGeneration || videoSeekDragging || pendingVideoSeek!==null
      || Number(clock.seekRevision??0)!==videoSeekRevision
      || videoLoading || !nativeAccompanimentReady) return;
  if(typeof clock.finished==='boolean')nativeVideoFinished=clock.finished;
  if(!videoWantsPlaying)return;
  if(!clock.playing) {
    if(clock.finished) {nativeVideoFinished=true;videoWantsPlaying=false;video.pause();$('#play-button').textContent='▶';}
    return;
  }
  const position=Number(clock.position);
  if(document.hidden || document.body.classList.contains('presentation-hidden')
      || !document.querySelector('#page-play')?.classList.contains('active') || !$('.video-card')?.getClientRects().length)return;
  if(Number.isFinite(position) && !video.seeking && Math.abs(video.currentTime-position)>.35) {
    ++videoClockCorrections;video.currentTime=position;
  }
  if(video.paused && !video.ended)video.play().catch(()=>{});
}
window.__JUCE__?.backend?.addEventListener('videoPlaybackClock',applyVideoPlaybackClock);
// The native player owns the single A/V clock. JS never loads or seeks a
// second decoder, and only sends explicit user commands and viewport changes.
function hideNativeVideoViewport() {
  nativeViewportLast='';
  nativeEvent('nativeVideoViewport',{session:videoWebSession,epoch:nativeViewportEpoch,revision:++nativeViewportRevision,visible:false});
}
function acceptNativeViewportEpoch(epoch) {
  if(!Number.isSafeInteger(epoch) || epoch<nativeViewportEpoch)return;
  if(epoch!==nativeViewportEpoch){nativeViewportEpoch=epoch;nativeViewportLast='';}
  scheduleNativeVideoViewport();
}
window.__JUCE__?.backend?.addEventListener('nativeVideoViewportReset',event=>{
  if(event?.session===videoWebSession)acceptNativeViewportEpoch(Number(event.epoch));
});
function scheduleNativeVideoViewport() {
  if(nativeViewportFrame || !nativeVideoEngine)return;
  nativeViewportFrame=requestAnimationFrame(reportNativeVideoViewport);
}
function reportNativeVideoViewport() {
  nativeViewportFrame=0;
  const surface=$('#native-video-surface'),rect=surface.getBoundingClientRect();
  const blocked=document.body.classList.contains('quick-tones-open') || Array.from(document.querySelectorAll('.dialog-backdrop,[role="dialog"],[role="menu"],[aria-modal="true"],.guide-overlay,.onboarding-overlay'))
    .some(e=>e.getClientRects().length>0 && getComputedStyle(e).visibility!=='hidden');
  const visible=nativeVideoEngine && nativeAccompanimentReady && !blocked && !document.hidden
    && !document.body.classList.contains('presentation-hidden')
    && $('#page-play').classList.contains('active') && rect.width>0 && rect.height>0;
  const payload={session:videoWebSession,epoch:nativeViewportEpoch,generation:videoGeneration,x:Math.round(rect.x),y:Math.round(rect.y),width:Math.round(rect.width),height:Math.round(rect.height),visible};
  const key=JSON.stringify(payload);
  if(key!==nativeViewportLast){nativeViewportLast=key;nativeEvent('nativeVideoViewport',{...payload,revision:++nativeViewportRevision});}
}
new ResizeObserver(scheduleNativeVideoViewport).observe($('#native-video-surface'));
window.addEventListener('resize',()=>{nativeViewportLast='';scheduleNativeVideoViewport();});
document.addEventListener('scroll',scheduleNativeVideoViewport,true);
const nativeViewportObserver=new MutationObserver(scheduleNativeVideoViewport);
nativeViewportObserver.observe(document.body,{attributes:true,attributeFilter:['class']});
function observeNativeVideoDialogs(root=document) {
  root.querySelectorAll('.dialog-backdrop,[role="dialog"],[role="menu"],[aria-modal="true"],.guide-overlay,.onboarding-overlay').forEach(e=>
    nativeViewportObserver.observe(e,{attributes:true,attributeFilter:['hidden','class','style']}));
}
observeNativeVideoDialogs();
// The guide creates its dialog after app.js has finished loading.
new MutationObserver(records=>{
  if(records.some(r=>Array.from(r.addedNodes).some(n=>n.nodeType===1))){
    observeNativeVideoDialogs();scheduleNativeVideoViewport();
  }
}).observe(document.body,{childList:true});
window.__JUCE__?.backend?.addEventListener('nativeVideoState',state=>{
  acceptNativeViewportEpoch(Number(state?.viewportEpoch));
  // Native playback survives a web document reload. Every authoritative
  // snapshot can repair a missed one-shot videoSelected event without seeking
  // or restarting the native player, or resetting an in-flight user seek.
  if(state?.engine==='vlc' && state.url && Number(state.generation)>=videoGeneration
      && (!nativeVideoEngine || Number(state.generation)!==videoGeneration))
    receiveVideoSelection({...state,restored:true});
  if(!nativeVideoEngine || Number(state?.generation)!==videoGeneration)return;
  if(Number(state.seekRevision)<videoSeekRevision)return;
  videoSeekRevision=Number(state.seekRevision)||0;
  nativeAccompanimentReady=!!state.ready;
  videoLoading=!state.ready && !state.error;
  nativeVideoDuration=Math.max(0,Number(state.duration)||0);
  nativeVideoPosition=Math.max(0,Number(state.position)||0);
  nativeVideoFinished=!!state.finished;
  videoWantsPlaying=!!state.playing;
  document.body.classList.toggle('video-playing',videoWantsPlaying);
  $('#play-button').textContent=videoWantsPlaying?'Ⅱ':'▶';
  $('#duration').textContent=formatTime(nativeVideoDuration);
  if(!videoSeekDragging && pendingVideoSeek===null){
    $('#current-time').textContent=formatTime(nativeVideoPosition);
    $('#seek').value=nativeVideoDuration?nativeVideoPosition/nativeVideoDuration*100:0;
  }
  $('#video-empty').style.display='none';
  if(state.error && state.error!==nativeVideoLastError)toast(state.error);
  nativeVideoLastError=state.error||'';
  scheduleNativeVideoViewport();
});
const videoVolumeControl=$('#video-volume');
let videoVolumeGesture=null,videoVolumeSendTimer=0;
function sendVideoVolume() {
  clearTimeout(videoVolumeSendTimer);videoVolumeSendTimer=0;
  nativeEvent('setVideoVolume',{value:Number(videoVolumeControl.value)/100});
}
function updateVideoVolume() {
  const value=Math.max(0,Math.min(100,Number(videoVolumeControl.value)||0));
  video.volume=value/100;
  $('#video-volume-value').textContent=`${value.toFixed(1).replace(/\.0$/,'')}%`;
  videoVolumeControl.setAttribute('aria-valuetext',`${value.toFixed(1)}%`);
  if(!videoVolumeSendTimer)videoVolumeSendTimer=setTimeout(sendVideoVolume,33);
}
videoVolumeControl.addEventListener('input',updateVideoVolume);
videoVolumeControl.addEventListener('change',sendVideoVolume);
// Relative dragging: touching the short track must not jump to its absolute
// position. A full sweep spans >=480 CSS px; arrows retain 0.1% fine steps.
videoVolumeControl.addEventListener('pointerdown',e=>{
  if(e.button!==0 || videoVolumeGesture)return;
  e.preventDefault();videoVolumeControl.focus();
  videoVolumeGesture={id:e.pointerId,x:e.clientX,value:Number(videoVolumeControl.value),
    span:Math.max(480,videoVolumeControl.getBoundingClientRect().width*4)};
  videoVolumeControl.setPointerCapture(e.pointerId);
});
videoVolumeControl.addEventListener('pointermove',e=>{
  if(!videoVolumeGesture || e.pointerId!==videoVolumeGesture.id)return;
  const g=videoVolumeGesture;
  videoVolumeControl.value=String(g.value+(e.clientX-g.x)*100/g.span);
  updateVideoVolume();
});
function endVideoVolumeGesture(e) {
  if(!videoVolumeGesture || (e?.pointerId!==undefined && e.pointerId!==videoVolumeGesture.id))return;
  const id=videoVolumeGesture.id;videoVolumeGesture=null;
  if(videoVolumeControl.hasPointerCapture(id))videoVolumeControl.releasePointerCapture(id);
  sendVideoVolume();
}
for(const type of ['pointerup','pointercancel','lostpointercapture'])videoVolumeControl.addEventListener(type,endVideoVolumeGesture);
window.addEventListener('blur',()=>endVideoVolumeGesture());
const videoCard = $('.video-card');
const fullscreenButton = $('#fullscreen');
function setNativeVideoExpanded(active) {
  hideNativeVideoViewport();
  document.body.classList.toggle('video-expanded',!!active);
  fullscreenButton.textContent=active?'退出全屏':'全屏';
  fullscreenButton.setAttribute('aria-label',active?'退出视频全屏':'视频全屏');
  nativeViewportLast='';scheduleNativeVideoViewport();
}
async function toggleFullscreen() {
  // A native VLC HWND cannot enter WebView2's DOM fullscreen/top layer.
  // FengYin's full layout already fills the display: expand the video inside
  // that same parent window, keeping native render and HTML controls aligned.
  if(nativeVideoEngine) {
    setNativeVideoExpanded(!document.body.classList.contains('video-expanded'));return;
  }
  try {
    if (document.fullscreenElement) await document.exitFullscreen();
    else if (videoCard.requestFullscreen) await videoCard.requestFullscreen();
    else return toast('当前系统不支持视频全屏');
  } catch (error) {
    toast(`无法切换全屏：${error?.message || '请重试'}`);
  }
}
fullscreenButton.addEventListener('click', toggleFullscreen);
document.addEventListener('fullscreenchange', () => {
  nativeViewportLast='';scheduleNativeVideoViewport();
  const active = document.fullscreenElement === videoCard;
  fullscreenButton.textContent = active ? '退出全屏' : '全屏';
  fullscreenButton.setAttribute('aria-label', active ? '退出视频全屏' : '视频全屏');
});

function formatTime(seconds) {
  if(!Number.isFinite(seconds)) return '00:00';
  const minutes = Math.floor(seconds / 60).toString().padStart(2,'0');
  const rest = Math.floor(seconds % 60).toString().padStart(2,'0');
  return `${minutes}:${rest}`;
}

$('#theme').addEventListener('change', event => { document.body.dataset.theme = event.target.value; updateStaticTheme(); requestVisualFrame(); });
// Keep the native select as the source of options; use a non-modal upward menu.
const themeButton=document.createElement('button'); themeButton.id='theme-button'; themeButton.type='button';
themeButton.textContent='主题 ⌃'; themeButton.setAttribute('aria-haspopup','menu'); themeButton.setAttribute('aria-expanded','false');
$('.global-controls').append(themeButton);
const themeMenu=document.createElement('div');themeMenu.className='inline-choice';themeMenu.hidden=true;themeMenu.setAttribute('role','menu');document.body.append(themeMenu);
$('#bend-range-button').setAttribute('aria-haspopup','menu');
function positionChoice(menu,button){const r=button.getBoundingClientRect();menu.style.maxHeight=Math.max(80,r.top-16)+'px';const box=menu.getBoundingClientRect();menu.style.left=Math.max(8,Math.min(innerWidth-box.width-8,r.right-box.width))+'px';menu.style.top=Math.max(8,r.top-box.height-7)+'px';}
function closeThemeMenu(){themeMenu.hidden=true;themeButton.setAttribute('aria-expanded','false');}
themeButton.onclick=()=>{if(!themeMenu.hidden)return closeThemeMenu();themeMenu.replaceChildren();for(const option of $('#theme').options){const b=document.createElement('button');b.textContent=option.textContent;b.setAttribute('role','menuitemradio');b.setAttribute('aria-checked',String(option.value===document.body.dataset.theme));b.onclick=()=>{$('#theme').value=option.value;$('#theme').dispatchEvent(new Event('change'));closeThemeMenu();themeButton.focus();};themeMenu.append(b);}themeMenu.hidden=false;themeButton.setAttribute('aria-expanded','true');positionChoice(themeMenu,themeButton);themeMenu.querySelector('[aria-checked=true]')?.focus();};
document.addEventListener('pointerdown',e=>{if(!themeMenu.contains(e.target)&&!themeButton.contains(e.target))closeThemeMenu();if(!$('#bend-range-dialog').contains(e.target)&&!$('#bend-range-button').contains(e.target))$('#bend-range-dialog').hidden=true;});
document.addEventListener('keydown',e=>{if(e.key==='Escape'){closeThemeMenu();$('#bend-range-dialog').hidden=true;}});
window.addEventListener('resize',()=>{closeThemeMenu();$('#bend-range-dialog').hidden=true;});
$('#master-volume').addEventListener('input', event => {
  $('#master-volume-value').textContent = `${event.target.value}%`;
  nativeEvent('setMasterVolume', {value:Number(event.target.value)/100});
});
function updateTransposeDisplay(targetKey) {
  selectedTargetKey = ((Math.round(Number(targetKey) || 0) % 12) + 12) % 12;
  $('#performance-key').value = String(selectedTargetKey);
  $('#performance-key').hidden=true;
  const label=$('#transpose-key > span');
  if(label)label.textContent=`演奏调：${['C','降D','D','降E','E','F','升F','G','降A','A','降B','B'][selectedTargetKey]}`;
}
function changePerformanceKey(value) {
  updateTransposeDisplay(value);
  nativeEvent('setKeyTranspose', {targetKey:selectedTargetKey});
  window.fengyinVoice?.say(`V026_${String(selectedTargetKey + 1).padStart(2,'0')}`);
}
$('#performance-key').addEventListener('change', event => changePerformanceKey(event.target.value));
$('#transpose-key').addEventListener('click',()=>{
  $('#transpose-current-name').textContent=['C调','降D调','D调','降E调','E调','F调','升F调','G调','降A调','A调','降B调','B调'][selectedTargetKey];
  $$('#transpose-grid [data-key]').forEach(b=>b.classList.toggle('active',Number(b.dataset.key)===selectedTargetKey));
  $('#transpose-dialog').hidden=false;
});
$('#transpose-grid').addEventListener('click',event=>{
  const button=event.target.closest('[data-key]');if(!button)return;
  changePerformanceKey(button.dataset.key);$('#transpose-dialog').hidden=true;
});
$('#cancel-transpose').addEventListener('click',()=>{$('#transpose-dialog').hidden=true;});
$('#transpose-dialog').addEventListener('click',event=>{if(event.target===$('#transpose-dialog'))$('#transpose-dialog').hidden=true;});
updateTransposeDisplay(0);
$('#music-tempo').addEventListener('click',event=>{
  const button=event.target.closest('[data-tempo]');if(!button)return;
  const mode=Number(button.dataset.tempo);window.__JUCE__?.backend?.emitEvent('setMusicTempo',{mode});
  window.fengyinVoice?.say(['MT_SLOW','MT_MEDIUM','MT_FAST'][mode]);
});
function renderBendRange(state) {
  bendState = state;
  const loaded = !!state.pluginLoaded && !state.pluginLoading;
  const key = state.bendPreferenceKey || '';
  const recommended = Number(state.bendRecommended) === 3 ? 3 : 1;
  $('#bend-range-button').disabled = !loaded;
  $('#bend-range-value').textContent = !loaded ? '—' : state.bendRangeApplied ? `±${state.bendRange}` : '待设置';
  $('#bend-range-button').title = loaded && !state.bendRangeApplied ? '音源弯音参数尚未确认，请打开设置查看' : '';
  if (key !== lastBendKey) {
    clearTimeout(bendHintTimer);
    $('#bend-hint').textContent = state.bendRangeRemembered && state.bendRangeApplied
      ? `已记住弯音范围±${state.bendRange}，可在此修改` : `推荐弯音范围±${recommended}，可在此修改`;
    $('#bend-hint').hidden = !loaded;
    bendHintTimer = setTimeout(() => { $('#bend-hint').hidden = true; }, 8000);
    lastBendKey = key;
    if (!$('#bend-range-dialog').hidden) {
      $('#bend-range-dialog').hidden = true;
      bendBusy = false;
      toast('乐器已切换，请重新打开弯音设置');
    }
  }
}
function showBendConfirmation() {
  const name = bendState.instrumentChineseName || '当前';
  $('#bend-confirm-message').textContent = `确定将${name}乐器的弯音范围修改为±${bendSelection}？点击确定系统将记住您的选择，下次再演奏${name}乐器时自动切换到您设定的弯音范围。`;
  $$('#bend-range-options button').forEach(button => {
    const selected=Number(button.dataset.bend)===bendSelection;
    button.classList.toggle('active',selected);button.setAttribute('aria-checked',String(selected));
    button.disabled=bendBusy;
  });
  $('#confirm-bend-range').disabled = bendBusy;
}
$('#bend-range-button').addEventListener('click', () => {
  if(!$('#bend-range-dialog').hidden){$('#bend-range-dialog').hidden=true;return;}
  bendDialogKey = bendState.bendPreferenceKey;
  bendSelection = bendState.bendRange || bendState.bendRecommended || 1;
  bendBusy = false;
  $('#bend-result-message').textContent = '';
  $('#reset-bend-range').disabled = false;
  showBendConfirmation();
  $('#bend-range-dialog').hidden = false;
  positionChoice($('#bend-range-dialog'),$('#bend-range-button'));
  $('#bend-range-options button.active')?.focus();
});
$('#bend-range-options').addEventListener('click', event => {
  const button = event.target.closest('[data-bend]');
  if (!button || bendBusy) return;
  bendSelection = Number(button.dataset.bend);
  showBendConfirmation();
  commitBendRange(false);
});
function commitBendRange(reset) {
  if (bendBusy || bendDialogKey !== bendState.bendPreferenceKey) return;
  bendBusy = true;
  showBendConfirmation();
  $('#confirm-bend-range').disabled = true;
  $('#reset-bend-range').disabled = true;
  $('#bend-result-message').textContent = '正在设置…';
  nativeEvent('setBendRange', {key:bendDialogKey, value:bendSelection, reset});
  if (!window.__JUCE__?.backend?.emitEvent) {
    renderBendRange({...bendState,bendRange:reset?bendState.bendRecommended:bendSelection,bendRangeApplied:true,bendRangeRemembered:!reset});
    $('#bend-range-dialog').hidden = true;
    bendBusy = false;
    toast('原型演示：安装版会修改音源并记住选择');
  }
}
$('#confirm-bend-range').addEventListener('click', () => commitBendRange(false));
$('#reset-bend-range').addEventListener('click', () => commitBendRange(true));
$('#close-bend-range').addEventListener('click', () => { $('#bend-range-dialog').hidden = true; $('#bend-range-button').focus(); });
$('#bend-range-dialog').addEventListener('keydown', event => { if (event.key === 'Escape') $('#close-bend-range').click(); });
window.__JUCE__?.backend?.addEventListener('bendRangeResult', result => {
  bendBusy = false;
  showBendConfirmation();
  $('#confirm-bend-range').disabled = false;
  $('#reset-bend-range').disabled = false;
  $('#bend-result-message').textContent = result.message || '';
  if (result.success) { $('#bend-range-dialog').hidden = true; toast(result.message); }
});
const performanceReverb = $('#performance-reverb');
performanceReverb.addEventListener('pointerdown', () => { reverbDragging = true; });
performanceReverb.addEventListener('pointerup', () => { reverbDragging = false; });
performanceReverb.addEventListener('pointercancel', () => { reverbDragging = false; });
performanceReverb.addEventListener('change', () => { reverbDragging = false; });
performanceReverb.addEventListener('input', event => {
  const value = Number(event.target.value);
  $('#performance-reverb-value').textContent = `${value}%`;
  $('#reverb-mix').value = String(value);
  $('#reverb-label').textContent = `强度 ${value}%`;
  nativeEvent('setPerformanceReverb', {value:value/100});
});

const techniqueDialog = $('#technique-dialog');
const techniqueHint = $('#technique-learn-hint');
function sourceDescription(mapping) {
  if (!mapping || !Number(mapping.sourceType)) return '未设置';
  if (Number(mapping.sourceType) === 1) return `控制器 CC${mapping.sourceNumber}`;
  if (Number(mapping.sourceType) === 2) return '吹嘴/按键压力（Aftertouch）';
  if (Number(mapping.sourceType) === 3) return '弯音摇杆';
  if (Number(mapping.sourceType) === 4) return `功能按键（音符 ${mapping.sourceNumber}）`;
  return '已设置';
}
function renderTechniqueMappings(force = false) {
  if (!techniquePlan.length && currentInstrument) techniquePlan = buildAdaptiveTechniquePlan(currentInstrument.instrumentKey, latestBackendState);
  techniquePlan.forEach(item => { if (!['off','hardware','breath'].includes(item.mode)) item.mode = 'breath'; });
  const list = $('#technique-list');
  const signature = JSON.stringify([techniquePlan, techniqueSensitivity, window.fengyinTechniqueLearning]);
  if (!force && signature === techniqueRenderSignature) return;
  if (!force && list.contains(document.activeElement) && /^(SELECT|INPUT)$/.test(document.activeElement.tagName)) return;
  techniqueRenderSignature = signature;
  if (!techniquePlan.length) {
    list.innerHTML = '<div class="adapter-empty"><b>请先选择音色方案</b><span>风吟只展示当前乐器真正需要的技巧</span></div>';
    $('#technique-more').hidden = true;
    return;
  }
  list.innerHTML = techniquePlan.map(item => {
    const hardware = item.mode === 'hardware';
    const mapped = sourceDescription(item) !== '未设置';
    const modeLabels = item.id === 'mute' ? {off:'关闭',hardware:'按键控制'} : item.id === 'vibrato' ? {off:'关闭',breath:'自动',hardware:'咬嘴控制'} : {off:'关闭',breath:'自动',hardware:'按键控制'};
    const heading = item.id === 'vibrato' ? '颤音' : item.id === 'growl' ? '嘶吼音' : item.name === '技巧音色' ? item.name : '其他技巧';
    return `<article class="technique-card ${item.featured ? '' : 'secondary'}" data-technique-id="${item.id}">
      <div class="technique-card-head"><div class="technique-card-title"><h3>${heading}</h3></div><span class="technique-card-state">${item.mode === 'off' ? '已关闭' : hardware ? (mapped ? '已映射' : '待映射') : '自动控制'}</span></div>
      <div class="technique-control"><div class="technique-mode-buttons" role="group" aria-label="${heading}控制方式">${Object.entries(modeLabels).map(([value,label]) => `<button type="button" data-technique-mode="${value}" aria-pressed="${item.mode === value}" ${!item.routeMissing ? '' : 'disabled'}>${label}</button>`).join('')}</div><div class="technique-map-actions" aria-label="${item.id === 'vibrato' ? '咬嘴控制' : '按键控制'}的映射设置" ${hardware ? '' : 'hidden'}><button type="button" class="learn-source" ${item.routeMissing ? 'disabled' : ''}>${Number(window.fengyinTechniqueLearning) === item.index ? '等待操作…' : mapped ? '重新映射' : '点击映射'}</button><button type="button" class="clear-source" ${mapped?'':'hidden'}>清除映射</button></div></div>
    </article>`;
  }).join('');
  $$('#technique-list .learn-source').forEach(button => button.addEventListener('click', event => {
    event.preventDefault(); event.stopPropagation();
    const item = techniquePlan.find(entry => entry.id === button.closest('.technique-card')?.dataset.techniqueId);
    if (item) startTechniqueLearning(item,false);
  }));
  $('#technique-more').hidden = true;
}
$('#technique-list').addEventListener('click',event=>{
  const button=event.target.closest('.clear-source');if(!button)return;
  const item=techniquePlan.find(entry=>entry.id===button.closest('[data-technique-id]')?.dataset.techniqueId);
  if(!item)return;
  nativeEvent('removeTechniqueMapping',{technique:item.index});
  window.fengyinVoice?.cleared(item.name);
  const saved=techniqueMappings.find(entry=>entry.id===item.id);
  if(saved){saved.sourceType=0;saved.sourceNumber=-1;saved.toggle=false;saved.mode=4;}
  item.sourceType=0;item.sourceNumber=-1;item.mode='off';window.fengyinTechniqueLearning=-1;
  renderTechniqueMappings(true);
});
function startTechniqueLearning(item,inline = false) {
  window.fengyinVoice?.learning(item.name);
  window.fengyinTechniqueLearning = item.index;
  nativeEvent('beginTechniqueLearn', {techniqueId:item.id,toggle:false});
  if (inline) {
    $('#adapter-inline-hint').textContent = `正在识别“${item.name}”：请操作一次对应按键、摇杆或吹嘴……`;
    renderAdapterTechniqueGrid();
    if (!window.__JUCE__?.backend?.emitEvent) window.setTimeout(() => {
      const target=techniquePlan.find(entry=>entry.id===item.id); if(target){target.sourceType=1;target.sourceNumber=74;}
      window.fengyinTechniqueLearning=-1; $('#adapter-inline-hint').textContent=`✓ “${item.name}”映射成功，切换音源后仍然有效。`; renderAdapterTechniqueGrid();
    },700);
    return;
  }
  techniqueHint.textContent = `正在识别“${item.name}”：请在 10 秒内操作一次吹嘴、摇杆或按键……`;
  $('#learning-dialog-title').textContent = `正在识别“${item.name}”`;
  $('#learning-dialog-message').textContent = '请操作一次希望用于这项技巧的按键、摇杆或吹嘴。普通音符和气息波动会自动忽略。';
  $('#learning-dialog-message').textContent += '弯音信号不参与映射；咬嘴若输出弯音，请在电吹管上改为连续CC。同一控制器映射两项技巧会同时触发。';
  $('#technique-learning-dialog').hidden = false;
  renderTechniqueMappings();
}

function adapterTechniqueItems() {
  return mergeBackendTechniquePlan(currentInstrument?.instrumentKey || latestBackendState.instrumentKey || '',latestBackendState,techniqueMappings);
  if (adapterMode === 'hardware') {
    // Hardware roles are device-level mappings and must stay available even
    // before an instrument has been loaded.  Do not derive this panel from
    // the current instrument technique plan: doing so hid function keys 2/3
    // whenever the audio engine temporarily reported no active plug-in.
    return mergeBackendTechniquePlan('', latestBackendState, techniqueMappings)
      .map(item => ({...item,mode:'hardware'}));
  }

  const continuous = new Set(['vibrato','growl','flutter','overblow','breathNoise','bowPressure']);
  const key = currentInstrument?.instrumentKey || latestBackendState.instrumentKey || '';
  return buildTechniquePlan(key)
    .filter(item => continuous.has(item.id))
    .map(item => {
      const saved = techniqueMappings.find(mapping => mapping.id === item.id);
      const mode = saved && [0,1,3].includes(Number(saved.mode)) ? 'breath' : 'off';
      return {...item,mode,enabled:mode !== 'off',
        strength:Number.isFinite(Number(saved?.strength)) ? Math.round(Number(saved.strength) * 100) : item.strength};
    });
}

function updateLocalTechniqueConfiguration(id, mode, strength) {
  let saved = techniqueMappings.find(mapping => mapping.id === id);
  if (!saved) {
    saved = {id,mode:mode === 'off' ? 4 : 1,strength};
    techniqueMappings.push(saved);
  } else {
    saved.mode = mode === 'off' ? 4 : 1;
    saved.strength = strength;
  }
}

function renderAdapterTechniqueGrid() {
  const grid = $('#adapter-technique-grid');
  if (!grid) return;
  techniquePlan = adapterTechniqueItems();
  const list=$('#technique-list');
  if (list.parentElement !== grid) { grid.replaceChildren(list); }
  renderTechniqueMappings();
  return;
  const hardware = adapterMode === 'hardware';
  const items = adapterTechniqueItems();
  const signature = JSON.stringify([items, adapterMode, window.fengyinTechniqueLearning]);
  if (signature === adapterRenderSignature) return;
  if (grid.contains(document.activeElement) && /^(SELECT|INPUT)$/.test(document.activeElement.tagName)) return;
  adapterRenderSignature = signature;
  grid.innerHTML = items.map(item => {
    const mapped = sourceDescription(item) !== '未设置';
    const learning = Number(window.fengyinTechniqueLearning) === Number(item.index);
    return `<article class="adapter-technique-item" data-technique-id="${escapeHtml(item.id)}">
      <header><span>${item.icon}</span><div><h3>${escapeHtml(item.name)}</h3><small>${escapeHtml(item.description)}</small></div></header>
      ${hardware
        ? `<div class="adapter-map-actions"><button class="adapter-map-source ${learning ? 'learning' : ''}" type="button">${learning ? '等待操作…' : mapped ? `✓ ${escapeHtml(sourceDescription(item))}` : '点击映射'}</button>${mapped?'<button class="adapter-clear-source" type="button">清除</button>':''}</div>`
        : `<div class="adapter-breath-controls"><label class="adapter-technique-toggle"><input type="checkbox" ${item.enabled === false ? '' : 'checked'}><span>${item.enabled === false ? '已关闭' : '已开启'}</span></label><small>曲线与深度由音源方案决定</small></div>`}
    </article>`;
  }).join('');
  $$('.adapter-map-source').forEach(button => button.addEventListener('click', event => {
    event.preventDefault();
    event.stopPropagation();
    const item = items.find(candidate => candidate.id === button.closest('[data-technique-id]')?.dataset.techniqueId);
    if (item) startTechniqueLearning(item,true);
  }));
  $$('.adapter-clear-source').forEach(button => button.addEventListener('click', event => {
    event.preventDefault();event.stopPropagation();
    const item=items.find(candidate=>candidate.id===button.closest('[data-technique-id]')?.dataset.techniqueId);
    if(!item)return;
    nativeEvent('removeTechniqueMapping',{technique:item.index});
    window.fengyinVoice?.cleared(item.name);
    item.sourceType=0;item.sourceNumber=-1;renderAdapterTechniqueGrid();
  }));
  $$('#adapter-technique-grid .adapter-technique-toggle input').forEach(input => input.addEventListener('change', event => {
    const id = input.closest('[data-technique-id]')?.dataset.techniqueId;
    const item = items.find(candidate => candidate.id === id);
    if (!item) return;
    item.enabled = event.target.checked;
    item.mode = item.enabled ? 'breath' : 'off';
    updateLocalTechniqueConfiguration(id,item.mode,Number(item.strength || 50)/100);
    nativeEvent('setTechniqueConfiguration',{techniqueId:id,mode:item.mode,strength:Number(item.strength || 50)/100});
    window.fengyinVoice?.speakText(`已${item.enabled?'开启':'关闭'}${item.name}气息控制。`);
    adapterRenderSignature=''; renderAdapterTechniqueGrid();
  }));
  $$('#adapter-technique-grid input[type="range"]').forEach(input => input.addEventListener('input', event => {
    const id = input.closest('[data-technique-id]')?.dataset.techniqueId;
    const item = items.find(candidate => candidate.id === id);
    const value = Number(event.target.value);
    event.target.nextElementSibling.textContent = `${value}%`;
    if (item) item.strength = value;
    updateLocalTechniqueConfiguration(id,'breath',value/100);
    nativeEvent('setTechniqueConfiguration',{techniqueId:id,mode:'breath',strength:value/100});
  }));
}

function setAdapterMatchPhase(phase, state = latestBackendState) {
  adapterMatchPhase = phase;
  const connected = !!state.deviceConnected;
  const guide = $('#adapter-inline-guide');
  const badge = $('#adapter-match-badge');
  const button = $('#adapter-match');
  const progress = $$('.adapter-match-progress span');
  guide.classList.toggle('matching', phase === 'waiting' || phase === 'matching');
  guide.classList.toggle('ready', phase === 'ready');
  $('#adapter-guide-detail').hidden = phase !== 'waiting' && phase !== 'matching';
  progress.forEach((item,index) => item.classList.toggle('active', index <= ({idle:0,waiting:0,matching:1,ready:2}[phase] ?? 0)));
  if (!connected) {
    badge.textContent = '等待连接'; badge.classList.remove('ready'); button.disabled = true; button.textContent = '开始匹配';
    $('#adapter-guide-title').textContent = '请先连接电吹管';
    $('#adapter-guide-detail').textContent = '';
    return;
  }
  button.disabled = false;
  if (phase === 'waiting') {
    badge.textContent = '等待吹气'; badge.classList.remove('ready'); button.textContent = '正在等待…';
    $('#adapter-guide-title').textContent = '请长吹一口气';
    $('#adapter-guide-detail').textContent = '不需要按任何音符键，自然地持续吹气即可。';
  } else if (phase === 'matching') {
    badge.textContent = '正在匹配'; badge.classList.remove('ready'); button.textContent = '正在匹配…';
    $('#adapter-guide-title').textContent = '正在自动匹配呼吸信号';
    $('#adapter-guide-detail').textContent = '请保持管身姿态固定，不咬嘴、不抬管，吹一个气息由弱到强的长音。';
  } else if (phase === 'ready') {
    badge.textContent = '匹配完成'; badge.classList.add('ready'); button.textContent = '重新匹配';
    $('#adapter-guide-title').textContent = '匹配完成，可以开始吹奏';
    $('#adapter-guide-detail').textContent = '';
  } else {
    badge.textContent = '尚未匹配'; badge.classList.remove('ready'); button.textContent = '开始匹配';
    $('#adapter-guide-title').textContent = '点击“开始匹配”';
    $('#adapter-guide-detail').textContent = '';
  }
}

function beginInlineAdapterMatch() {
  if (!latestBackendState.deviceConnected) return toast('请先连接电吹管');
  window.clearTimeout(adapterMatchTimer);
  setAdapterMatchPhase('waiting');
  nativeEvent('beginBreathMatch');
  if (window.__JUCE__?.backend?.emitEvent) return;
  adapterMatchTimer = window.setTimeout(() => {
    setAdapterMatchPhase('matching');
    adapterMatchTimer = window.setTimeout(() => setAdapterMatchPhase('ready'), 1150);
  }, 700);
}

$('#adapter-match')?.addEventListener('click', beginInlineAdapterMatch);
$$('[data-adapter-mode]').forEach(button => button.addEventListener('click', () => {
  adapterMode = button.dataset.adapterMode;
  $$('[data-adapter-mode]').forEach(item => item.classList.toggle('active', item === button));
  globalTechniqueMode = adapterMode;
  $('#adapter-inline-hint').textContent = adapterMode === 'hardware'
    ? '映射一次即可用于所有音源；请选择一项技巧后操作对应按键、摇杆或吹嘴。'
    : '选择需要由气息控制的技巧；曲线与深度在音源插件中调整并保存到方案。';
  renderAdapterTechniqueGrid();
}));
function openTechniqueDialog() {
  if (!currentPluginLoaded || !currentInstrument) return toast('请先选择并加载一个音色方案');
  if (!techniquePlan.length) techniquePlan = buildAdaptiveTechniquePlan(currentInstrument.instrumentKey, latestBackendState);
  $('#technique-dialog-title').textContent = '全局演奏技巧';
  $('#technique-dialog-subtitle').textContent = `${latestBackendState.deviceName || '当前电吹管'} · 映射一次，切换乐器继续使用`;
  $('#technique-ready').textContent = `已开启 ${techniquePlan.filter(item => item.mode !== 'off' && item.pluginSupported).length} 项`;
  techniqueDialog.hidden = false;
  techniqueHint.textContent = '保留音源方案的技巧曲线；硬件控制请点击“点击映射”。';
  renderTechniqueMappings();
}
$('#close-technique-dialog').addEventListener('click', () => {
  nativeEvent('cancelTechniqueLearn');
  techniqueDialog.hidden = true;
});
$('#close-technique-dialog-top').addEventListener('click', () => {
  nativeEvent('cancelTechniqueLearn');
  techniqueDialog.hidden = true;
});
document.addEventListener('keydown', event => {
  if (event.key === 'Escape' && !techniqueDialog.hidden) {
    nativeEvent('cancelTechniqueLearn');
    techniqueDialog.hidden = true;
  }
});
techniqueDialog.addEventListener('click', event => {
  if (event.target === techniqueDialog) {
    nativeEvent('cancelTechniqueLearn');
    techniqueDialog.hidden = true;
  }
});
$('#technique-list').addEventListener('click',event=>{
  const button=event.target.closest('[data-technique-mode]');
  if(!button || button.disabled)return;
  const item=techniquePlan.find(entry=>entry.id===button.closest('.technique-card')?.dataset.techniqueId);
  if(!item)return;
  item.mode=button.dataset.techniqueMode;
  nativeEvent('setTechniqueConfiguration',{techniqueId:item.id,mode:item.mode,strength:item.strength/100});
  window.fengyinVoice?.say(item.id==='vibrato' ? (item.mode==='off'?'V058_01':item.mode==='hardware'?'NEW_VIB_HARDWARE':'V057_01')
    :item.id==='growl'?(item.mode==='off'?'V058_02':item.mode==='hardware'?'NEW_GROWL_HARDWARE':'V057_02')
    :item.mode==='off'?'NEW_OTHER_OFF':'NEW_OTHER_ON');
  renderTechniqueMappings(true);
});
window.__JUCE__?.backend?.addEventListener('techniqueConfigurationResult',result=>{
  if(result?.success===false)toast(result.message || '技巧配置未生效');
});
$('#technique-list').addEventListener('change', event => {
  const card = event.target.closest('.technique-card');
  if (!card) return;
  const item = techniquePlan.find(entry => entry.id === card.dataset.techniqueId);
  if (!item) return;
  if (event.target.matches('.growl-sensitivity')) {
    techniqueSensitivity = event.target.value;
    techniqueHint.textContent = `嘶吼音灵敏度：${event.target.selectedOptions[0].textContent}`;
    nativeEvent('setGrowlSensitivity', {sensitivity:techniqueSensitivity});
    return;
  }
  if (event.target.matches('.technique-control select')) {
    item.mode = event.target.value;
    const wording = {off:'已关闭此技巧映射',breath:'已改用气息控制，使用音源方案中的曲线',hardware:'请点击“点击映射”'};
    techniqueHint.textContent = wording[item.mode];
    nativeEvent('setTechniqueConfiguration', {techniqueId:item.id,mode:item.mode,strength:item.strength/100});
    renderTechniqueMappings(true);
    if (item.mode === 'hardware' && sourceDescription(item) === '未设置') startTechniqueLearning(item,false);
  }
  if (event.target.matches('input[type="range"]')) {
    item.strength = Number(event.target.value);
    nativeEvent('setTechniqueConfiguration', {techniqueId:item.id,mode:item.mode,strength:item.strength/100});
    const label = event.target.closest('.technique-strength')?.querySelector('b');
    if (label) label.textContent = item.strength < 40 ? '自然' : item.strength < 70 ? '适中' : '明显';
  }
});
$('#technique-more').addEventListener('click', event => {
  const expanded = $('#technique-list').classList.toggle('show-more');
  event.currentTarget.textContent = expanded ? '收起次要技巧⌃' : '显示更多可用技巧⌄';
});
$$('[data-tech-global]').forEach(button => button.addEventListener('click', () => {
  globalTechniqueMode = button.dataset.techGlobal;
  $$('[data-tech-global]').forEach(item => item.classList.toggle('active', item === button));
  techniquePlan.forEach(item => { item.mode = globalTechniqueMode; });
  $('#technique-mode-summary').textContent = globalTechniqueMode === 'hardware' ? '硬件控制' : '气息控制';
  techniquePlan.forEach(item => nativeEvent('setTechniqueConfiguration', {techniqueId:item.id,mode:item.mode,strength:item.strength/100}));
  renderTechniqueMappings();
}));
$('#cancel-technique-learning').addEventListener('click', () => {
  nativeEvent('cancelTechniqueLearn');
  window.fengyinTechniqueLearning = -1;
  $('#technique-learning-dialog').hidden = true;
  techniqueHint.textContent = '已取消识别，原有技巧设置保持不变。';
  renderTechniqueMappings();
});

let recordingRequestPending=false;
function closeRecordingMenu(){ $('#record-menu').hidden=true;$('#record').setAttribute('aria-expanded','false'); }
function positionRecordingMenu(){
  const menu=$('#record-menu'),anchor=$('#record').getBoundingClientRect();
  menu.style.left=`${Math.max(8,Math.min(innerWidth-menu.offsetWidth-8,anchor.left))}px`;
  menu.style.top=`${Math.max(8,Math.min(innerHeight-menu.offsetHeight-8,anchor.top-menu.offsetHeight-8))}px`;
}
$('#record').addEventListener('click',()=>{
  if(latestBackendState.recordingSaving||recordingRequestPending)return;
  if(recording||screenRecordingBusy){nativeEvent(screenRecordingBusy?'toggleScreenRecording':'toggleRecording');return;}
  const open=$('#record-menu').hidden;$('#record-menu').hidden=!open;$('#record').setAttribute('aria-expanded',String(open));if(open){positionRecordingMenu();$('#record-menu button').focus();}
});
$$('[data-record-kind]').forEach(button=>button.addEventListener('click',()=>{
  closeRecordingMenu();$('#record-success').hidden=true;
  if(!window.__JUCE__?.backend)return toast('请在Windows风吟中使用录制功能');
  if(recording||screenRecordingBusy||recordingRequestPending)return;
  recordingRequestPending=true;$('#record').textContent='准备中 · 取消';
  // Start native preparation immediately. Native prompt playback runs alongside it.
  nativeEvent(button.dataset.recordKind==='video'?'toggleScreenRecording':'toggleRecording',window.fengyinVoice?.recordingPreferences?.()||{announce:false,volume:.45});
}));
document.addEventListener('pointerdown',event=>{if(!event.target.closest('#record-menu,.record-control'))closeRecordingMenu();});
document.addEventListener('keydown',event=>{if(event.key==='Escape')closeRecordingMenu();});
window.addEventListener('resize',()=>{closeRecordingMenu();$('#record-success').hidden=true;});
window.__JUCE__?.backend?.addEventListener('screenRecordingMessage',message=>toast(String(message)));
$('#recording-manager').addEventListener('click', () => nativeEvent('showRecordings'));
window.__JUCE__?.backend?.addEventListener('recordingResult',result=>{
  const banner=$('#record-success');banner.hidden=!result?.success&&!result?.canRecover;if(banner.hidden)return;
  $('#record-recover').hidden=!result?.canRecover;
  banner.querySelector('span').textContent=String(result.message||'录制成功！在这里查看录制文件');
  const anchor=$('#recording-manager').getBoundingClientRect(),left=Math.max(8,Math.min(innerWidth-banner.offsetWidth-8,anchor.left));
  banner.style.left=`${left}px`;banner.style.top=`${Math.max(8,anchor.top-banner.offsetHeight-12)}px`;
  banner.querySelector('i').style.left=`${Math.max(12,Math.min(banner.offsetWidth-18,anchor.left+anchor.width/2-left))}px`;
  if(result.cleanupWarning)toast('录制已保存，但部分临时文件未能清理，请在“临时文件”中查看。');
});
$('#record-result-close').addEventListener('click',()=>{$('#record-success').hidden=true;});
$('#record-recover').addEventListener('click',()=>{$('#record-success').hidden=true;nativeEvent('resumeScreenRecording');});

function trialTimeText(seconds) {
  const safe = Math.max(0, Number(seconds) || 0);
  return `${Math.ceil(safe / 60)}分钟`;
}

function renderLicenseState(state = {}) {
  const permanent = !!state.activated;
  const trialActive = !!state.trialActive;
  const trialExpired = !!state.trialExpired;
  const lock = $('#license-lock');
  $('#license-title').classList.toggle('green', permanent || trialActive);
  $('#start-trial').hidden = permanent || trialActive || trialExpired;
  $('#license-code').hidden=permanent;$('#activate').hidden=permanent;
  $('.license-code-label').hidden=permanent;
  if (permanent) {
    $('#license-title').textContent = '已永久激活';
    lock.hidden = true;
    return;
  }
  if (trialActive) {
    $('#license-title').textContent = `完整试用中 · 剩余${trialTimeText(state.trialRemainingSeconds)}`;
    lock.hidden = true;
    return;
  }
  lock.hidden = false;
  $('#license-lock-primary').dataset.action = trialExpired ? 'activate' : 'trial';
  $('#license-lock-primary').textContent = trialExpired ? '输入永久激活码' : '开始1小时完整试用';
  $('#license-lock-kicker').textContent = trialExpired ? '完整试用已结束' : '欢迎使用风吟';
  $('#license-lock-title').textContent = trialExpired ? '激活后继续演奏' : '开始1小时完整试用';
  $('#license-lock-message').textContent = trialExpired
    ? '试用期间保存的设置会继续保留。完成永久激活后，全部功能会立即恢复。'
    : '请准备好电吹管和软音源后再开始。点击后将连续计算1小时，全部功能均可使用。';
  $('#license-title').textContent = trialExpired ? '1小时试用已结束' : '可开始1小时完整试用';
}

function startFullTrial() {
  if (window.__JUCE__?.backend?.emitEvent) {
    nativeEvent('startTrial');
    toast('正在开始1小时完整试用…');
    return;
  }
  const startedAt = Date.now();
  localStorage.setItem('fengyin-prototype-trial-1.1.6-start', String(startedAt));
  renderLicenseState({trialActive:true, trialRemainingSeconds:60*60});
  toast('1小时完整试用已开始');
}

$('#start-trial').addEventListener('click', startFullTrial);
$('#license-lock-primary').addEventListener('click', event => {
  if (event.currentTarget.dataset.action === 'activate') return nativeEvent('showActivation');
  startFullTrial();
});
$('#license-lock-activate').addEventListener('click', () => nativeEvent('showActivation'));
$('#license-lock-copy').addEventListener('click', () => {
  nativeEvent('copyMachineCode');
  toast('机器码已复制');
});

$('#activate').addEventListener('click', () => {
  if (window.__JUCE__?.backend?.emitEvent) {
    const code = $('#license-code').value.trim();
    if (code.length < 8) return toast('请输入完整激活码');
    nativeEvent('activate', {code});
    toast('正在验证激活码…');
    return;
  }
  const code = $('#license-code').value.trim();
  if (code.length < 8) return toast('请输入至少 8 位激活码');
  localStorage.setItem('fengyin-prototype-license','active');
  renderLicenseState({activated:true});
  toast('原型激活成功');
});

$('#scan-swam').addEventListener('click', () => {
  nativeEvent('scanPlugins'); $('#scan-title').textContent='正在扫描…'; $('#scan-label').textContent='请稍候，扫描不会阻塞演奏'; toast('正在扫描 SWAM 与 VST3 音源…');
  if (!window.__JUCE__?.backend?.emitEvent) window.setTimeout(() => {
    availableInstruments = [
      {name:'SWAM Soprano Sax 3',label:'高音萨克斯 · SWAM Soprano Sax 3',chineseName:'高音萨克斯',isSwam:true},
      {name:'SWAM Trumpet',label:'小号 · SWAM Trumpet',chineseName:'小号',isSwam:true},
      {name:'Kontakt 8',label:'Kontakt 8',chineseName:'Kontakt',isSwam:false},
      {name:'三体音源',label:'三体音源',chineseName:'三体音源',isSwam:false}
    ];
    $('#instrument-select').innerHTML = '<optgroup label="可用的 SWAM 音源"><option value="0">高音萨克斯 · SWAM Soprano Sax 3</option><option value="1">小号 · SWAM Trumpet</option></optgroup><optgroup label="暂未支持"><option value="2">Kontakt 8（暂未支持）</option><option value="3">三体音源（暂未支持）</option></optgroup>';
    $('#scan-title').textContent='重新扫描音源'; $('#scan-label').textContent='已分类可用与暂未支持音源'; $('#plugin-status').textContent='找到 2 个可用 SWAM 音源；2 个其他音源已列为暂未支持。';
    renderPresets();
  }, 650);
});
$('#load-instrument').addEventListener('click', () => {
  const index = Number($('#instrument-select').value);
  if (!Number.isInteger(index) || index < 0) return toast('请先扫描并选择一个乐器音源');
  if (!isSupportedInstrument(availableInstruments[index])) return toast('当前版本尚未支持所选音源，请关注后续版本升级。');
  nativeEvent('loadPlugin', {index}); toast('正在加载所选音源…');
});
$('#instrument-select').addEventListener('change', () => {
  if (customToneMode !== 'create') return;
  customSourceReady=false;
  if (!$('#instrument-select').value) {
    $('#open-instrument').disabled=true; $('#custom-effects-step').hidden=true; return;
  }
  const index = Number($('#instrument-select').value);
  if (!Number.isInteger(index) || index < 0) {
    $('#open-instrument').disabled=true; $('#custom-effects-step').hidden=true; return;
  }
  if (!isSupportedInstrument(availableInstruments[index])) {
    $('#instrument-select').value=''; return toast('当前版本尚未支持此音源');
  }
  pendingPresetNavigation={kind:'custom-create',name:availableInstruments[index]?.chineseName||availableInstruments[index]?.name||'乐器音源'};
  $('#instrument-load-state').textContent='正在加载音源…'; $('#open-instrument').disabled=true;
  nativeEvent('loadPlugin',{index});
});
$('#open-instrument').addEventListener('click', () => { toast('正在打开音源界面…'); nativeEvent('openPlugin'); });
$('#load-effect').addEventListener('click', () => {
  if($('#effect-select').value==='')return toast('请先扫描并选择一个外部效果器');
  const index = Number($('#effect-select').value);
  if (!Number.isInteger(index) || index < 0) return toast('请先选择一个外部效果器');
  nativeEvent('loadEffect', {index}); toast('正在加载效果器…');
});
$('#remove-effect').addEventListener('click', () => { nativeEvent('removeEffect'); toast('已移除外部效果器'); });
$('#open-effect').addEventListener('click', () => { toast('正在打开效果器界面…'); nativeEvent('openEffect'); });

function updateBuiltinEffects() {
  const eq = Number($('#eq-tone').value);
  const reverb = Number($('#reverb-mix').value);
  const warmth = Number($('#tone-warmth')?.value || 20);
  $('#eq-label').textContent = eq === 0 ? '自然' : `${eq > 0 ? '明亮度 +' : '温暖度 +'}${Math.abs(eq)}`;
  $('#reverb-label').textContent = `强度 ${reverb}%`;
  performanceReverb.value = String(reverb);
  $('#performance-reverb-value').textContent = `${reverb}%`;
  nativeEvent('setBuiltinEffects', {eq:eq/100,warmth:warmth/100,reverb:reverb/100});
}
['#eq-tone','#reverb-mix'].forEach(id => $(id).addEventListener('input', updateBuiltinEffects));
$('#wind-status-pill')?.addEventListener('click', () => openMidiConnection());
const audioControls = ['#audio-driver-select','#audio-output-select','#audio-rate-select','#audio-buffer-select'].map(id => $(id));
let audioSettingsApplying = false;

function setAudioOptions(select, values, selected, label) {
  const items = Array.isArray(values) ? values : [];
  select.replaceChildren(...items.map(value => {
    const option = document.createElement('option');
    option.value = String(value);
    option.textContent = label(value);
    return option;
  }));
  if (items.length && items.map(String).includes(String(selected))) select.value = String(selected);
}

function setAudioControlsBusy(busy) {
  audioSettingsApplying = busy;
  audioControls.forEach(control => { control.disabled = busy; });
  $('#configure-asio4all').disabled = busy;
  $('#audio-apply-status').classList.toggle('audio-saving', busy);
  if (busy) $('#audio-apply-status').textContent = '正在应用并保存声音设置…';
}

function applyInlineAudioSettings(driverChanged = false) {
  if (audioSettingsApplying) return;
  if (!window.__JUCE__?.backend?.emitEvent) return toast('原型模式：安装版会立即应用并保存');
  setAudioControlsBusy(true);
  nativeEvent('applyAudioSettings', {
    type:$('#audio-driver-select').value,
    output:driverChanged ? '' : $('#audio-output-select').value,
    sampleRate:driverChanged ? 48000 : Number($('#audio-rate-select').value),
    bufferSize:driverChanged ? 128 : Number($('#audio-buffer-select').value)
  });
}

$('#audio-driver-select').addEventListener('change', () => applyInlineAudioSettings(true));
$('#configure-asio4all').addEventListener('click', () => nativeEvent('configureAsio4All'));


['#audio-output-select','#audio-rate-select','#audio-buffer-select'].forEach(id => $(id).addEventListener('change', () => applyInlineAudioSettings(false)));

const superLatencyDialog = $('#super-latency-confirm-dialog');
$('#super-latency-optimize').addEventListener('click', () => {
  $('#super-latency-ack').checked = false;
  $('#confirm-super-latency').disabled = true;
  superLatencyDialog.hidden = false;
});
$('#super-latency-ack').addEventListener('change', event => {
  $('#confirm-super-latency').disabled = !event.target.checked;
});
$('#cancel-super-latency').addEventListener('click', () => { superLatencyDialog.hidden = true; });
$('#confirm-super-latency').addEventListener('click', () => {
  if (!$('#super-latency-ack').checked) return;
  superLatencyDialog.hidden = true;
  $('#super-latency-optimize').disabled = true;
  $('#super-latency-status').textContent = '正在请求管理员权限…';
  if (!window.__JUCE__?.backend?.emitEvent) return toast('Windows 安装版才能修改系统驱动');
  nativeEvent('startSuperLowLatencyOptimisation');
});
$('#restore-audio-driver').addEventListener('click', () => {
  if (!window.confirm('恢复原声卡驱动后需重启电脑，确定继续？')) return;
  $('#restore-audio-driver').disabled = true;
  nativeEvent('restoreOriginalAudioDriver');
});

function renderSuperLowLatencyState(state = {}) {
  const running = state.running === true || state.state === 'running';
  const button = $('#super-latency-optimize');
  button.disabled = running || state.supported === false || (state.eligibleDeviceFound === false && !state.canRestore);
  button.textContent = running ? '正在优化…' : state.restartRequired ? '重启后生效' : '开始优化';
  $('#restore-audio-driver').hidden = state.canRestore !== true;
  $('#restore-audio-driver').disabled = running;
  const message = state.message || (state.supported === false ? '仅支持 Windows 板载声卡' : '正在检查系统声卡…');
  $('#super-latency-status').textContent = message;
  $('#super-latency-status').classList.toggle('audio-error', state.state === 'error');
}

window.__JUCE__?.backend?.addEventListener('superLowLatencyState', renderSuperLowLatencyState);
window.__JUCE__?.backend?.addEventListener('pauseForDriverChange', () => {
  videoWantsPlaying = false;
  ++videoPlayRequest;
  video.pause();
});


function renderSmartAdapter(state) {
  const connected = !!state.deviceConnected;
  const loaded = !!state.pluginLoaded;
  const matched = state.deviceMatched === true || state.breathMatched === true;
  const instrumentKey = state.instrumentKey || currentInstrument?.instrumentKey || inferInstrumentKey(state.pluginName || '');
  if (loaded && (!techniquePlan.length || currentInstrument?.instrumentKey !== instrumentKey)) techniquePlan = buildAdaptiveTechniquePlan(instrumentKey, state);
  $('#adapter-kicker').textContent = !connected ? '等待连接设备'
    : matched ? '已完成设备匹配'
    : state.deviceRecognized ? '已识别型号，等待匹配' : '发现新的电吹管';
  $('#adapter-device-name').textContent = connected ? (state.deviceName || state.deviceProfileName || '电吹管已连接') : '尚未连接电吹管';
  $('#adapter-summary').textContent = '';
  if (!connected) setAdapterMatchPhase('idle',state);
  else if (state.breathMatching) setAdapterMatchPhase('matching',state);
  else if (matched) setAdapterMatchPhase('ready',state);
  else if (adapterMatchPhase === 'idle' || adapterMatchPhase === 'ready') setAdapterMatchPhase('idle',state);
  renderAdapterTechniqueGrid();
}

let backendControlSignature = '';
function backendControlsChanged(state) {
  const controls = {...state};
  // These counters/meters have a fast path below. They must not rebuild every
  // hidden settings panel, artwork and style selector at the audio-meter rate.
  for (const key of ['breath','instrumentPeak','outputLeftPeak','outputRightPeak',
    'audioCpu','audioXruns','callbackOverruns','overloadSamples','droppedMidiEvents','note','noteReceived','videoClock']) delete controls[key];
  const signature = JSON.stringify(controls);
  if (signature === backendControlSignature) return false;
  backendControlSignature = signature;
  return true;
}
function updateRealtimePresentation(state) {
  const active = !!(state.pluginLoaded && state.deviceConnected) || !!originalVideoSource || !!state.recording;
  if (document.body.classList.contains('realtime-rendering') !== active)
    document.body.classList.toggle('realtime-rendering', active);
}
function renderBackendMeters(packet,replace=false) {
  const state=replace?packet:{...latestBackendState,...packet};
  const visualSignature = value=>JSON.stringify([Math.round(Number(value.breath||0)*127),Math.round(Number(value.instrumentPeak||0)*200),Math.round(Number(value.outputLeftPeak||0)*200),Math.round(Number(value.outputRightPeak||0)*200),!!value.pluginLoaded]);
  const visualChanged = visualSignature(state) !== visualSignature(latestBackendState);
  latestBackendState = state || {};
  hardwareBreath = state.deviceConnected ? Math.max(0,Math.min(100,Number(state.breath || 0)*100)) : null;
  if (Number.isFinite(Number(state.audioCpu))) setTextIfChanged($('#cpu'),`${Math.round(Number(state.audioCpu)*100)}%`);
  setTextIfChanged(document.querySelector('.sidebar-status small'),!state.deviceConnected ? '点击连接'
    : state.noteReceived ? '气息与音符信号正常'
    : Number(state.breath || 0) > 0.02 ? '已收到气息，尚未收到音符' : '已连接，等待吹奏信号');
  updateRealtimePresentation(state);
  if (visualChanged) requestVisualFrame();
}
window.__JUCE__?.backend?.addEventListener('backendMeterState',packet=>renderBackendMeters(packet));
window.__JUCE__?.backend?.addEventListener('backendState', state => {
  if(state.videoClock)applyVideoPlaybackClock(state.videoClock);
  renderBackendMeters(state,true);
  if (!backendControlsChanged(state)) return;
  renderBendRange(state);
  $$('#music-tempo [data-tempo]').forEach(button=>{const active=Number(button.dataset.tempo)===Number(state.musicTempoMode??1);button.classList.toggle('active',active);button.setAttribute('aria-pressed',String(active));});
  $('#music-tempo').title='调整自动颤音介入和风吟混响预延迟；咬颤保持音色方案设置。';
  if(!localStorage.getItem('fy-music-tempo-v1-guide')){localStorage.setItem('fy-music-tempo-v1-guide','yes');toast('乐曲速度默认中速：慢速低于80，中速80至120，快速高于120 BPM。可调整自动颤音和混响预延迟。');window.fengyinVoice?.say('MT_GUIDE');}
  if (!sessionStorage.getItem('fy-performance-key-v3-notice')) {
    sessionStorage.setItem('fy-performance-key-v3-notice','yes');
    toast('演奏调模式：请先将电吹管设置为C调，再选择演奏调。');
  }
  if (state.pitchMappingNeedsRemap && !sessionStorage.getItem('fy-pitch-remap-'+state.deviceIdentifier)) {
    sessionStorage.setItem('fy-pitch-remap-'+state.deviceIdentifier,'yes');
    toast('旧弯音技巧映射已解除。弯音专用，请用CC、压力或按键重新映射技巧。');
  }
  if (typeof state.compactMode === 'boolean' && state.compactMode !== compactMode) applyWindowMode(state.compactMode);
  const connected = !!state.deviceConnected;
  const adapterDeviceKey = String(state.deviceIdentifier || state.deviceId || state.deviceName || state.deviceProfileName || 'connected-device');
  const hasExplicitMatchState = typeof state.deviceMatched === 'boolean' || typeof state.breathMatched === 'boolean';
  const needsFirstMatch = connected && (hasExplicitMatchState
    ? !(state.deviceMatched === true || state.breathMatched === true)
    : state.deviceRecognized === false);
  if (needsFirstMatch && adapterAutoOpenedFor !== adapterDeviceKey) {
    adapterAutoOpenedFor = adapterDeviceKey;
    adapterMatchPhase = 'idle';
    showPage('wind');
  }
  if (Number.isFinite(Number(state.latency))) $('#latency').textContent = Number(state.latency) > 0 ? `${Number(state.latency).toFixed(1)} ms` : '—';
  if (!audioSettingsApplying && Number.isFinite(Number(state.latency))) {
    const value = Number(state.latency);
    $('#audio-latency-value').textContent = value > 0 ? `${value.toFixed(1)} ms · ${value <= 10 ? '优秀' : value <= 20 ? '良好' : '偏高'}` : '音频未运行 · 暂无延迟数据';
    $('#audio-latency-value').classList.toggle('green', value > 0 && value <= 20);
  }
  const targetKey = Number(state.targetKey || 0);
  if (targetKey !== selectedTargetKey) updateTransposeDisplay(targetKey);
  if (Number.isFinite(Number(state.growlSensitivity)))
    techniqueSensitivity = ['easy','standard','hard'][Number(state.growlSensitivity)] || 'standard';
  const backendReverb = Math.round(Math.max(0,Math.min(.6,Number(state.reverbMix ?? .28)))*100);
  if (!reverbDragging && document.activeElement !== performanceReverb && document.activeElement !== $('#reverb-mix')) {
    performanceReverb.value = String(backendReverb);
    $('#performance-reverb-value').textContent = `${backendReverb}%`;
    $('#reverb-mix').value = String(backendReverb);
    $('#reverb-label').textContent = `强度 ${backendReverb}%`;
  }
  const backendEq = Math.round(Math.max(-1,Math.min(1,Number(state.eqTone ?? .2)))*100);
  if (document.activeElement !== $('#eq-tone')) {
    $('#eq-tone').value = String(backendEq);
    $('#eq-label').textContent = backendEq === 0 ? '自然' : `${backendEq > 0 ? '明亮度 +' : '温暖度 +'}${Math.abs(backendEq)}`;
  }
  techniqueMappings = Array.isArray(state.techniqueMappings) ? state.techniqueMappings : [];
  window.fengyinTechniqueLearning = Number(state.techniqueLearning ?? -1);
  if (!techniqueDialog.hidden) renderTechniqueMappings();
  const sideTitle = document.querySelector('.sidebar-status b');
  const sideText = document.querySelector('.sidebar-status small');
  const windStatusPill = $('#wind-status-pill');
  const windStatusText = $('#wind-status-text');
  if (sideTitle) sideTitle.textContent = connected ? `${state.deviceName || '电吹管'} · ${String(state.deviceIdentifier||'').startsWith('ble:')?'蓝牙':'USB'}` : '尚未连接电吹管';
  if (sideText) sideText.textContent = !connected ? '点击连接'
    : state.noteReceived ? '气息与音符信号正常'
    : Number(state.breath || 0) > 0.02 ? '已收到气息，尚未收到音符' : '已连接，等待吹奏信号';
  if (windStatusText) windStatusText.textContent = connected ? '电吹管已连接' : '未连接电吹管';
  if (windStatusPill) windStatusPill.classList.toggle('disconnected', !connected);
  currentPluginLoaded = !!state.pluginLoaded;
  const sound = $('#sound-name');
  if (sound) sound.textContent = currentPluginLoaded
    ? (state.instrumentChineseName || state.pluginName)
    : '尚未选择音色';
  $('#source-plugin-name').textContent = currentPluginLoaded ? (state.pluginName || '') : '请先到“音色方案”选择并加载';
  currentInstrument = currentPluginLoaded ? {
    name:state.pluginName,
    chineseName:state.instrumentChineseName || state.pluginName,
    instrumentKey:state.instrumentKey || inferInstrumentKey(state.pluginName),
    brand:state.pluginBrand
  } : null;
  if (currentPluginLoaded)
    techniquePlan = mergeBackendTechniquePlan(currentInstrument.instrumentKey, state, techniqueMappings);
  if (currentPluginLoaded && (state.instrumentKey || state.pluginName))
    showInstrumentArtwork(state.instrumentKey || inferInstrumentKey(state.pluginName), state.instrumentChineseName || state.pluginName);
  else if (!currentPluginLoaded) {
    currentInstrument = null;
    techniquePlan = mergeBackendTechniquePlan('', state, techniqueMappings);
    clearInstrumentArtwork();
  }
  renderInstrumentModelSwitcher();
  refreshInlineExpert();
  renderSmartAdapter(state);
  const progress = Math.round((Number(state.scanProgress) || 0) * 100);
  $('#scan-title').textContent = state.scanning ? `正在扫描 ${progress}%` : '重新扫描音源';
  $('#scan-label').textContent = state.scanning ? '请稍候，找到后自动分类' : '点击查找 SWAM / VST3';
  $('#scan-swam').disabled = !!state.scanning;
  if (state.pluginStatus) $('#plugin-status').textContent = state.pluginStatus;
  const instrumentState = currentPluginLoaded ? `已加载：${state.instrumentChineseName || state.pluginName}` : (state.pluginLoading ? '正在加载乐器音源…' : '当前未加载乐器音源');
  $('#instrument-load-state').textContent = instrumentState;
  $('#instrument-load-state').classList.toggle('loaded', currentPluginLoaded);
  $('#load-instrument').classList.toggle('loaded', currentPluginLoaded);
  $('#load-instrument').textContent = state.pluginLoading ? '正在加载…' : (currentPluginLoaded ? '✓ 更换音源' : '加载音源');
  const effectLoaded = !!state.effectLoaded;
  $('#load-effect').classList.toggle('loaded', effectLoaded);
  $('#load-effect').textContent = state.effectLoading ? '正在加载…' : '添加效果器';
  $('#load-effect').disabled = !!state.effectLoading || !!state.pluginLoading || !state.pluginLoaded;
  availableInstruments = Array.isArray(state.instruments) ? state.instruments : [];
  publishedTones = Array.isArray(state.publishedTones) ? state.publishedTones : [];
  availableEffects = Array.isArray(state.effects) ? state.effects : [];
  savedPresets = Array.isArray(state.presets) ? state.presets : [];
  if (!state.presetEditing && editingPresetName) {
    editingPresetName = '';
  }
  const signature = JSON.stringify([availableInstruments,availableEffects]);
  if (signature !== pluginListSignature) {
    pluginListSignature = signature;
    if (availableInstruments.length) {
      // QinEngine is a container, not an individual playable instrument. Its
      // instruments enter this editor through their saved instrument cards.
      const supported = availableInstruments.map((item,index) => ({item,index}))
        .filter(({item}) => isSupportedInstrument(item));
      const swam = supported.filter(({item}) => item.brand === 'swam' || item.isSwam === true);
      const kong = supported.filter(({item}) => item.brand === 'kong');
      const kontakt = supported.filter(({item}) => item.brand === 'kontakt' || /kontakt/i.test(item.name || item.label || ''));
      const unsupported = availableInstruments.map((item,index) => ({item,index})).filter(({item}) => !isSupportedInstrument(item));
      $('#instrument-select').innerHTML = `<option value="">请选择音源</option>${swam.length ? `<optgroup label="西洋乐器 · SWAM">${swam.map(({item,index}) => `<option value="${index}">${escapeHtml(item.label || item.name)}</option>`).join('')}</optgroup>` : ''}${kong.length ? `<optgroup label="中国民乐 · 空音">${kong.map(({item,index}) => `<option value="${index}">${escapeHtml(item.label || item.name)}</option>`).join('')}</optgroup>` : ''}${unsupported.length ? `<optgroup label="暂未支持">${unsupported.map(({item,index}) => `<option class="unsupported-plugin" value="${index}">${escapeHtml(item.label || item.name)}（暂未支持）</option>`).join('')}</optgroup>` : ''}`;
      if (kontakt.length) {
        const group = document.createElement('optgroup'); group.label = '采样音源 · Kontakt';
        for (const {item,index} of kontakt) {
          const option = document.createElement('option'); option.value = String(index);
          option.textContent = item.label || item.name; group.appendChild(option);
        }
        $('#instrument-select').appendChild(group);
      }
    } else $('#instrument-select').innerHTML = '<option value="">未找到乐器音源</option>';
    $('#effect-select').innerHTML = availableEffects.length
      ? availableEffects.map((name,index) => `<option value="${index}">${escapeHtml(name)}</option>`).join('')
      : '<option value="">未找到外部效果器（可不选）</option>';
  }
  renderToneStyleSwitcher();
  if (currentPluginLoaded && (state.activeToneVariantId || state.toneStyleId)) {
    const failedStyle = state.swamToneIncomplete ? `${state.toneStyleId}|${state.instrumentModelName}` : '';
    if (failedStyle && window.lastFailedSwamStyle !== failedStyle)
      toast('音色参数或型号未完全匹配，请确认使用 SWAM 3.9.4，并导出当前音色参数供检查。');
    window.lastFailedSwamStyle = failedStyle;
    const styles = toneVariantsForCurrentInstrument();
    const wantedVariant = state.activeToneVariantId || state.toneStyleId;
    const backendStyleIndex = styles.findIndex(style => style.id === wantedVariant || style.id === state.toneStyleId);
    if (backendStyleIndex >= 0 && backendStyleIndex !== currentToneStyleIndex) {
      currentToneStyleIndex = backendStyleIndex;
      renderToneStyleSwitcher();
    }
  }
  const nextPresetSignature = JSON.stringify([publishedTones,savedPresets, availableInstruments, state.kongInstruments, state.kongLibrary, !!state.scanning,state.activeToneVariantId,!!state.pluginLoading]);
  if (nextPresetSignature !== presetListSignature) {
    presetListSignature = nextPresetSignature;
    renderPresets();
  }
  if(state.pluginLoaded) $('#sound-name').textContent=state.activePresetName || `${state.instrumentChineseName || ''}-${toneStylesForInstrument(state.instrumentKey).find(s=>s.id===state.toneStyleId)?.name || ''}`;
  renderLicenseState(state);
  if (state.machineCode) $('#machine-code').textContent = `本机识别码：${state.machineCode}`;
  if (state.appVersion) {
    const versionNode = document.querySelector('.prototype-note');
    if (versionNode) versionNode.textContent = `风吟 ${state.appVersion} · 本地运行，不会保存或上传个人资料。`;
  }
  recording = !!state.recording;
  recordingRequestPending=false;
  screenRecordingBusy=!!state.screenRecordingBusy;
  applyPreparedVideoSource();
  const saving=!!state.recordingSaving||!!state.screenRecordingStopping||!!state.audioRecordingStopping;
  $('#record').textContent=saving?'正在保存，请稍等':state.recordingPreparing?'准备中 · 取消':recording||screenRecordingBusy?'正在录制 · 停止':'● 开始录制';
  $('#record').classList.toggle('saving',saving);$('#record').classList.toggle('recording',recording||screenRecordingBusy);
  $('#record').setAttribute('aria-busy',String(saving||!!state.recordingPreparing));
  $('#record').setAttribute('aria-haspopup',recording||screenRecordingBusy?'false':'menu');
  $('#record').style.color = recording||screenRecordingBusy ? 'var(--danger)' : '';
  $('#record').disabled=saving;
});

$('#copy-machine-code').addEventListener('click', () => {
  if (!latestBackendState.machineCode) return toast('尚未取得本机识别码');
  nativeEvent('copyMachineCode');
  toast('机器码已复制，请发送给安装人员');
});
window.__JUCE__?.backend?.addEventListener('kongLibraryResult', result => toast(result?.message || '音色库位置已更新'));
$('#cancel-container-instrument')?.addEventListener('click', () => {
  nativeEvent('cancelContainerInstrument');
  $('#container-instrument-dialog').hidden=true;
});
$('#open-container-instrument')?.addEventListener('click', () => {
  $('#open-container-instrument').disabled=true;
  $('#confirm-container-instrument').disabled=true;
  $('#container-instrument-status').textContent='正在打开 QinEngineV3……';
  nativeEvent('beginContainerInstrument',{adapter:'kong-v3'});
});
$('#confirm-container-instrument')?.addEventListener('click', () => {
  const naming=!$('#container-instrument-name').hidden;
  const name=naming?$('#container-instrument-name').value.trim():'';
  if (naming && !name) return toast('请填写音色名称');
  $('#confirm-container-instrument').disabled=true;
  $('#container-instrument-status').textContent=naming?'正在保存音色……':'正在保存当前乐器和奏法……';
  nativeEvent('commitContainerInstrument',{name});
});
window.__JUCE__?.backend?.addEventListener('containerInstrumentResult', result => {
  const status=$('#container-instrument-status');
  if (status) status.textContent=result?.message || '';
  if (result?.stage === 'ready') {
    $('#confirm-container-instrument').disabled=false;
    $('#container-instrument-status').textContent='请在空音窗口选择并试听；选好后关闭原厂窗口，再返回创建乐器。';
  } else if (result?.stage === 'captured' && result?.success) {
    $('#container-instrument-name').hidden=false;
    $('#container-instrument-name').focus();
    $('#open-container-instrument').hidden=true;
    $('#confirm-container-instrument').disabled=false;
    $('#confirm-container-instrument').textContent='保存音色';
  } else if (result?.stage === 'saved' && result?.success) {
    $('#container-instrument-dialog').hidden=true;
    toast(result.message || '空音乐器已添加');
  } else if (!result?.success) {
    $('#open-container-instrument').disabled=false;
    $('#confirm-container-instrument').disabled=false;
    toast(result?.message || '空音乐器添加失败');
  }
});
function completeToneLoad(){
  toneLoadInFlight=false;
  const next=queuedToneLoad;queuedToneLoad=null;
  if(next)setTimeout(next,100);
}
window.__JUCE__?.backend?.addEventListener('pluginLoadResult', result => {
  completeToneLoad();
  if (result?.success) expertDirty = false;
  if (!pendingPresetNavigation || !['plugin','published','custom-create'].includes(pendingPresetNavigation.kind)) return;
  const pending = pendingPresetNavigation;
  pendingPresetNavigation = null;
  if (!result.success) return toast(result.message || '音色载入失败');
  if (pending.kind === 'custom-create') {
    customSourceReady=true;
    $('#open-instrument').disabled=false; $('#custom-effects-step').hidden=false;
    $('#instrument-load-state').textContent='';
    if($('#studio-instrument-name')) $('#studio-instrument-name').value=/空音|Qin/i.test(pending.name)?'':pending.name;
    refreshInlineExpert();
    return toast('音源已加载，可以边吹边调整');
  }
  if (pending.kind === 'plugin' && pending.styleId) nativeEvent('setToneStyle', {id:pending.styleId});
  closeQuickTones();
  showPage('play');
  toast(`已应用：${pending.name}`);
  window.fengyinVoice?.tone(pending.name);
});
window.__JUCE__?.backend?.addEventListener('presetLoadResult', result => {
  completeToneLoad();
  if (result?.success) expertDirty = false;
  if (!pendingPresetNavigation || !['preset','edit'].includes(pendingPresetNavigation.kind)) return;
  const pending = pendingPresetNavigation;
  pendingPresetNavigation = null;
  if (!result.success) return toast(result.message || '音色方案载入失败');
  if (pending.kind === 'edit') {
    enterCustomToneEdit(pending.name);
    showPage('chain');
    toast(`可以修改“${pending.name}”，完成后点击保存修改`);
  } else {
    closeQuickTones();
    showPage('play');
    toast(`已载入：${pending.name}`);
    window.fengyinVoice?.tone(pending.name);
  }
});
window.__JUCE__?.backend?.addEventListener('licenseStateResult', result => {
  renderLicenseState(result);
  if (result.activated) $('#license-code').value = '';
  toast(result.message || (result.featuresUnlocked ? '可以开始使用' : '无法继续'));
});
window.__JUCE__?.backend?.addEventListener('editorResult', message => toast(String(message || '')));
function receiveVideoSelection(result) {
  if (!result?.url) return;
  if(Number(result.generation)<videoGeneration)return;
  if(nativeVideoEngine && result.engine==='vlc' && Number(result.generation)===videoGeneration
      && originalVideoSource===result.url)return;
  videoLoading = true;
  videoWantsPlaying = false;
  ++videoPlayRequest;
  videoGeneration = Number(result.generation);
  videoSeekRevision=0;
  nativeVideoFinished=false;
  preparedVideoSource=null;restoringVideoPosition=null;usingPreparedVideo=false;
  originalVideoSource=result.url;
  updateRealtimePresentation(latestBackendState);
  lastVideoSyncAt = 0;
  videoSeekDragging=false;pendingVideoSeek=null;
  $('#seek').value = 0;
  $('#current-time').textContent = '00:00';
  $('#duration').textContent = '00:00';
  selectedVideoFile = null;
  triedVideoDataFallback = true;
  nativeAccompanimentReady = false;
  video.pause();
  video.muted = true;
  video.style.display = 'none';
  $('#video-empty').style.display = 'grid';
  if (videoUrl) URL.revokeObjectURL(videoUrl);
  videoUrl = '';
  nativeVideoEngine=result.engine==='vlc';
  nativeVideoDuration=0;nativeVideoPosition=0;nativeVideoLastError='';
  $('.video-card').classList.toggle('native-vlc',nativeVideoEngine);
  $('#native-video-surface').hidden=!nativeVideoEngine;
  if(nativeVideoEngine){
    video.removeAttribute('src');video.load();
    $('#video-name').textContent=result.name||'伴奏视频';
    $('#change-video').textContent='更换视频';
    document.body.classList.remove('video-playing');
    nativeViewportLast='';scheduleNativeVideoViewport();
    if(!result.restored)toast('视频已选择，正在读取播放信息…');
    return;
  }
  video.src = result.url;
  video.load();
  $('#video-name').textContent = result.name || '伴奏视频';
  $('#change-video').textContent = '更换视频';
  toast('正在载入视频并准备伴奏音轨…');
}
window.__JUCE__?.backend?.addEventListener('videoSelected',receiveVideoSelection);
window.__JUCE__?.backend?.addEventListener('videoAudioState', result => {
  if (Number(result?.generation) !== videoGeneration) return;
  nativeAccompanimentReady = !!result?.ready;
  if(nativeVideoEngine){
    videoLoading=false;
    if(result?.message)toast(result.message);
    scheduleNativeVideoViewport();return;
  }
  // 后台音轨准备成功后，网页只负责画面，声音统一由风吟输出并进入录音。
  video.muted = true;
  if (result?.message) toast(result.message);
});
window.__JUCE__?.backend?.addEventListener('audioSettingsState', state => {
  setAudioControlsBusy(false);
  setAudioOptions($('#audio-driver-select'), state?.types, state?.type, value => {
    const name = String(value);
    if (name.includes('ASIO4ALL')) return name;
    if (/Low Latency|低延迟/i.test(name)) return `${name}（推荐·共享）`;
    return `${name}（共享）`;
  });
  setAudioOptions($('#audio-output-select'), state?.outputs, state?.output, value => String(value));
  setAudioOptions($('#audio-rate-select'), state?.sampleRates, Number(state?.sampleRate), value => `${Math.round(Number(value))} Hz${Number(value) === 48000 ? '（推荐）' : ''}`);
  setAudioOptions($('#audio-buffer-select'), state?.bufferSizes, Number(state?.bufferSize), value => `${Number(value)}${Number(value) === 128 ? '（推荐）' : Number(value) === 256 ? '（更稳定）' : ''}`);
  const asioBridge = String(state?.type || '').includes('ASIO4ALL');
  $('#audio-output-select').hidden = asioBridge;
  // Keep the recovery entry available even after ASIO startup falls back.
  $('#configure-asio4all').hidden = !asioBridge;
  $('#configure-asio4all').innerHTML = '打开 ASIO4ALL 面板（选择输出端点） <span aria-hidden="true">↗</span>';
  $('#configure-asio4all').style.marginTop = asioBridge ? '0' : '10px';
  $('#audio-output-label').htmlFor = asioBridge ? 'configure-asio4all' : 'audio-output-select';
  const latency = Number(state?.latency);
  const latencyText = Number.isFinite(latency) && latency > 0 ? `${latency.toFixed(1)} ms · ${latency <= 10 ? '优秀' : latency <= 20 ? '良好' : '偏高'}` : '音频未运行 · 暂无延迟数据';
  $('#audio-latency-value').textContent = latencyText;
  $('#audio-latency-value').classList.toggle('green', Number.isFinite(latency) && latency > 0 && latency <= 20);
  const status = $('#audio-apply-status');
  status.textContent = state?.success === false ? (state.message || '声音设置未能应用，请重新选择。') : '';
  status.hidden = state?.success !== false;
  status.classList.toggle('audio-error', state?.success === false);
  if (state?.message) toast(state.message);
});
window.__JUCE__?.backend?.addEventListener('techniqueLearnResult', result => {
  window.fengyinTechniqueLearning = -1;
  $('#technique-learning-dialog').hidden = true;
  techniqueHint.textContent = result.message || (result.success ? '识别成功' : '没有识别到控制信号');
  $('#adapter-inline-hint').textContent = result.message || (result.success ? '✓ 映射成功，切换音源后仍然有效。' : '没有识别到控制信号，请重新操作一次。');
  toast(techniqueHint.textContent);
  renderTechniqueMappings();
  renderAdapterTechniqueGrid();
});
window.__JUCE__?.backend?.addEventListener('breathMatchResult', result => {
  window.clearTimeout(adapterMatchTimer);
  if (result?.success) {
    latestBackendState.deviceMatched = true;
    latestBackendState.breathMatched = true;
    setAdapterMatchPhase('ready',latestBackendState);
  } else {
    adapterMatchPhase = 'idle';
    setAdapterMatchPhase('idle',latestBackendState);
  }
  if (result?.message) toast(result.message);
});
nativeEvent('webReady',{videoSession:videoWebSession});
nativeEvent('requestSuperLowLatencyStatus');
renderSmartAdapter({});
renderTechniqueMappings();
clearInstrumentArtwork();
$('.prototype-note').textContent = '风吟 1.1.35 · 本地运行，不会上传个人资料。';
if (!window.__JUCE__?.backend?.emitEvent) {
  availableInstruments = [
    {name:'SWAM Violin',label:'SWAM Violin',chineseName:'小提琴',instrumentKey:'violin',brand:'swam',isSwam:true},
    {name:'SWAM Soprano Sax 3',label:'SWAM Soprano Sax 3',chineseName:'高音萨克斯',instrumentKey:'soprano-sax',brand:'swam',isSwam:true}
  ];
  savedPresets = [
    {id:'review-custom-1',name:'我的抒情小提琴',instrumentKey:'violin',instrumentChineseName:'小提琴',brand:'swam'},
    {id:'review-custom-2',name:'现场独奏',instrumentKey:'soprano-sax',instrumentChineseName:'高音萨克斯',brand:'swam'}
  ];
  currentPluginLoaded = true;
  currentInstrument = {name:'SWAM Violin',chineseName:'小提琴',instrumentKey:'violin'};
  techniquePlan = mergeBackendTechniquePlan('violin',{},[]).map(item => ({...item,mode:'hardware'}));
  latestBackendState = {
    deviceConnected:true,deviceName:'Yamaha YDS-150',deviceIdentifier:'review-yds150',deviceRecognized:false,deviceMatched:false,
    pluginLoaded:true,pluginName:'SWAM Violin',instrumentChineseName:'小提琴',instrumentKey:'violin',
    instrumentModels:[{id:'violin-a',name:'小提琴 A'},{id:'violin-b',name:'小提琴 B'},{id:'violin-c',name:'小提琴 C'}],
    activeToneVariantId:'builtin:violin:natural',techniqueMappings:[]
  };
  $('#sound-name').textContent = '小提琴';
  showInstrumentArtwork('violin','小提琴');
  renderInstrumentModelSwitcher(true);
  renderToneStyleSwitcher(true);
  renderPresets();
  renderSmartAdapter(latestBackendState);
}
// 浏览器原型使用 localStorage 模拟试用；正式软件等待本地授权服务回传状态。
// 这样已永久激活的用户启动时不会短暂看到“开始试用”遮罩。
if (!window.__JUCE__?.backend?.emitEvent) {
  if(new URLSearchParams(location.search).has('review')) renderLicenseState({activated:true});
  else if(localStorage.getItem('fengyin-prototype-license') === 'active') renderLicenseState({activated:true});
  else {
    const trialStartedAt = Number(localStorage.getItem('fengyin-prototype-trial-1.1.6-start') || 0);
    const trialRemaining = Math.max(0, 60*60 - Math.floor((Date.now() - trialStartedAt) / 1000));
    renderLicenseState(trialStartedAt > 0 && trialRemaining > 0
      ? {trialActive:true, trialRemainingSeconds:trialRemaining}
      : {trialExpired:trialStartedAt > 0});
  }
}

const canvas = $('#spectrum');
const ctx = canvas.getContext('2d');
const breathWaveCanvas = $('#breath-wave');
const breathWaveContext = breathWaveCanvas.getContext('2d');
const atmosphereCanvas = $('#theme-atmosphere');
const atmosphereContext = atmosphereCanvas.getContext('2d');
const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
let tick = 0;

function seeded(index, salt = 0) {
  const value = Math.sin((index + 1) * 91.173 + salt * 47.77) * 43758.5453;
  return value - Math.floor(value);
}

function drawPetal(x, y, size, rotation, color) {
  atmosphereContext.save();
  atmosphereContext.translate(x,y);
  atmosphereContext.rotate(rotation);
  atmosphereContext.fillStyle = color;
  atmosphereContext.beginPath();
  atmosphereContext.moveTo(0,-size);
  atmosphereContext.bezierCurveTo(size*.85,-size*.45,size*.72,size*.62,0,size);
  atmosphereContext.bezierCurveTo(-size*.72,size*.62,-size*.85,-size*.45,0,-size);
  atmosphereContext.fill();
  atmosphereContext.restore();
}

function drawLotus(x, baseY, scale, time, accent, accent2) {
  const sway = Math.sin(time*.42+x*.003)*5;
  atmosphereContext.save();
  atmosphereContext.translate(x,baseY);
  atmosphereContext.strokeStyle = accent;
  atmosphereContext.lineWidth = 1.5;
  atmosphereContext.globalAlpha = .24;
  atmosphereContext.beginPath();
  atmosphereContext.moveTo(0,22*scale);
  atmosphereContext.quadraticCurveTo(sway,0,sway*.7,-34*scale);
  atmosphereContext.stroke();
  atmosphereContext.translate(sway*.7,-36*scale);
  atmosphereContext.rotate(sway*.008);
  atmosphereContext.fillStyle = accent2;
  [-1,0,1].forEach(index => {
    atmosphereContext.save();
    atmosphereContext.rotate(index*.55);
    atmosphereContext.beginPath();
    atmosphereContext.ellipse(0,-7*scale,6*scale,13*scale,0,0,Math.PI*2);
    atmosphereContext.fill();
    atmosphereContext.restore();
  });
  atmosphereContext.restore();
}

function drawThemeAtmosphere(ratio, accent, accent2) {
  const bounds = atmosphereCanvas.getBoundingClientRect();
  const pixelWidth = Math.max(1,Math.round(bounds.width*ratio));
  const pixelHeight = Math.max(1,Math.round(bounds.height*ratio));
  if (atmosphereCanvas.width !== pixelWidth || atmosphereCanvas.height !== pixelHeight) {
    atmosphereCanvas.width = pixelWidth;
    atmosphereCanvas.height = pixelHeight;
  }
  atmosphereContext.setTransform(ratio,0,0,ratio,0,0);
  atmosphereContext.clearRect(0,0,bounds.width,bounds.height);
  const theme = document.body.dataset.theme;
  if (theme === 'minimal' || reducedMotion || document.body.classList.contains('low-performance')) return;
  const time = performance.now()/1000;
  const width = bounds.width;
  const height = bounds.height;
  atmosphereContext.lineCap = 'round';
  atmosphereContext.globalCompositeOperation = 'screen';

  if (theme === 'spring') {
    atmosphereContext.setLineDash([38,28]);
    for(let index=0;index<3;index++) {
      const y = height*(.23+index*.22)+Math.sin(time*.28+index)*14;
      atmosphereContext.strokeStyle = accent;
      atmosphereContext.globalAlpha = .16;
      atmosphereContext.lineWidth = 1.7;
      atmosphereContext.beginPath();
      atmosphereContext.moveTo(-40,y);
      atmosphereContext.bezierCurveTo(width*.26,y-36,width*.62,y+35,width+40,y-8);
      atmosphereContext.stroke();
    }
    atmosphereContext.setLineDash([]);
    atmosphereContext.globalAlpha = .34;
    for(let index=0;index<18;index++) {
      const x = (seeded(index,1)*width+time*(5+seeded(index,2)*6))%(width+50)-25;
      const y = (seeded(index,3)*height+time*(8+seeded(index,4)*7))%(height+40)-20;
      drawPetal(x+Math.sin(time*.5+index)*18,y,3+seeded(index,5)*3,time*.25+index,accent2);
    }
  } else if (theme === 'summer') {
    for(let index=0;index<4;index++) {
      const radius = (time*7+index*37)%145;
      atmosphereContext.strokeStyle = index%2 ? accent2 : accent;
      atmosphereContext.globalAlpha = .21*(1-radius/145);
      atmosphereContext.lineWidth = 1.7;
      atmosphereContext.beginPath();
      atmosphereContext.ellipse(width*(.18+index*.21),height*(.32+(index%2)*.38),radius,radius*.32,0,0,Math.PI*2);
      atmosphereContext.stroke();
    }
    drawLotus(width*.17,height*.82,.95,time,accent,accent2);
    drawLotus(width*.87,height*.74,.72,time+1.7,accent,accent2);
  } else if (theme === 'autumn') {
    atmosphereContext.strokeStyle = accent2;
    atmosphereContext.lineWidth = 1.2;
    atmosphereContext.globalAlpha = .25;
    for(let index=0;index<38;index++) {
      const speed = 20+seeded(index,2)*16;
      const y = (seeded(index,3)*height+time*speed)%(height+35)-20;
      const x = (seeded(index,1)*width-time*6+height-y*.1)%(width+40)-20;
      atmosphereContext.beginPath();
      atmosphereContext.moveTo(x,y);
      atmosphereContext.lineTo(x-7,y+18+seeded(index,4)*9);
      atmosphereContext.stroke();
    }
  } else if (theme === 'winter') {
    atmosphereContext.fillStyle = accent;
    atmosphereContext.globalAlpha = .3;
    for(let index=0;index<38;index++) {
      const y = (seeded(index,3)*height+time*(7+seeded(index,4)*8))%(height+24)-12;
      const x = seeded(index,1)*width+Math.sin(time*.3+index)*14;
      atmosphereContext.beginPath();
      atmosphereContext.arc(x,y,1.1+seeded(index,5)*2.2,0,Math.PI*2);
      atmosphereContext.fill();
    }
    const snowX = width-78;
    const snowY = height-28;
    atmosphereContext.globalAlpha = .24;
    atmosphereContext.fillStyle = accent;
    atmosphereContext.beginPath(); atmosphereContext.arc(snowX,snowY-18,25,0,Math.PI*2); atmosphereContext.fill();
    atmosphereContext.beginPath(); atmosphereContext.arc(snowX,snowY-54,17,0,Math.PI*2); atmosphereContext.fill();
    atmosphereContext.fillStyle = accent2;
    atmosphereContext.fillRect(snowX-21,snowY-76,42,6);
    atmosphereContext.fillRect(snowX-14,snowY-91,28,17);
  } else if (theme === 'gold') {
    for(let index=0;index<5;index++) {
      const center = width*(.33+index*.085)+Math.sin(time*.18+index)*16;
      atmosphereContext.fillStyle = accent;
      atmosphereContext.globalAlpha = .055+Math.sin(time*.35+index)*.014;
      atmosphereContext.beginPath();
      atmosphereContext.moveTo(center-16,0);
      atmosphereContext.lineTo(center+18,0);
      atmosphereContext.lineTo(center+190,height);
      atmosphereContext.lineTo(center-150,height);
      atmosphereContext.closePath();
      atmosphereContext.fill();
    }
    atmosphereContext.fillStyle = accent2;
    atmosphereContext.globalAlpha = .25;
    for(let index=0;index<24;index++) {
      const x = seeded(index,1)*width+Math.sin(time*.2+index)*9;
      const y = (seeded(index,2)*height-time*(3+seeded(index,3)*4)+height)%height;
      atmosphereContext.fillRect(x,y,1.5,1.5);
    }
  } else if (theme === 'neon') {
    for(let index=0;index<3;index++) {
      const x = (width*(.2+index*.3)+Math.sin(time*.2+index)*80);
      atmosphereContext.strokeStyle = index%2 ? accent2 : accent;
      atmosphereContext.globalAlpha = .1;
      atmosphereContext.lineWidth = 42;
      atmosphereContext.beginPath();
      atmosphereContext.moveTo(x,-30);
      atmosphereContext.lineTo(x+Math.sin(time*.16+index)*210,height+30);
      atmosphereContext.stroke();
    }
    for(let index=0;index<22;index++) {
      const barWidth = width/22;
      const barHeight = 10+Math.abs(Math.sin(index*.48+time*.72))*42;
      atmosphereContext.fillStyle = index%2 ? accent2 : accent;
      atmosphereContext.globalAlpha = .13;
      atmosphereContext.fillRect(index*barWidth+2,height-barHeight,Math.max(4,barWidth-7),barHeight);
    }
  } else if (theme === 'china-red') {
    atmosphereContext.strokeStyle = accent2;
    atmosphereContext.globalAlpha = .16;
    atmosphereContext.lineWidth = 2;
    for(let index=0;index<2;index++) {
      atmosphereContext.beginPath();
      atmosphereContext.moveTo(-30,height*(.35+index*.32));
      atmosphereContext.bezierCurveTo(width*.28,height*(.22+index*.28),width*.66,height*(.55+index*.18),width+40,height*(.3+index*.28));
      atmosphereContext.stroke();
    }
    atmosphereContext.fillStyle = accent2;
    atmosphereContext.globalAlpha = .27;
    for(let index=0;index<19;index++) {
      const x = seeded(index,2)*width+Math.sin(time*.2+index)*10;
      const y = (seeded(index,4)*height-time*(4+seeded(index,5)*3)+height)%height;
      atmosphereContext.beginPath(); atmosphereContext.arc(x,y,1+seeded(index,1)*1.4,0,Math.PI*2); atmosphereContext.fill();
    }
  }
  atmosphereContext.globalAlpha = 1;
  atmosphereContext.globalCompositeOperation = 'source-over';
}

function drawBreathWave(ratio, breath, accent, accent2) {
  const bounds = breathWaveCanvas.getBoundingClientRect();
  const pixelWidth = Math.max(1,Math.round(bounds.width*ratio));
  const pixelHeight = Math.max(1,Math.round(bounds.height*ratio));
  if (breathWaveCanvas.width !== pixelWidth || breathWaveCanvas.height !== pixelHeight) {
    breathWaveCanvas.width = pixelWidth;
    breathWaveCanvas.height = pixelHeight;
  }
  breathWaveContext.setTransform(ratio,0,0,ratio,0,0);
  breathWaveContext.clearRect(0,0,bounds.width,bounds.height);
  const centerX = bounds.width/2;
  const centerY = bounds.height/2+3;
  const energy = breath/100;
  [[78,accent,.48,0],[97,accent2,.34,1.7],[117,accent,.2,3.4]].forEach(([base,color,alpha,offset],ring) => {
    breathWaveContext.beginPath();
    for(let index=0;index<=120;index++) {
      const angle = index/120*Math.PI*2;
      const wobble = (2+energy*(7+ring*2))*Math.sin(angle*5+tick*2.1+offset) + Math.sin(angle*9-tick*1.25)*energy*3;
      const distance = base+energy*(9+ring*7)+wobble;
      const x = centerX+Math.cos(angle)*distance;
      const y = centerY+Math.sin(angle)*distance*.84;
      if(index===0) breathWaveContext.moveTo(x,y); else breathWaveContext.lineTo(x,y);
    }
    breathWaveContext.closePath();
    breathWaveContext.strokeStyle = color;
    breathWaveContext.globalAlpha = alpha+energy*.22;
    breathWaveContext.lineWidth = 1.2+energy*1.5;
    breathWaveContext.shadowColor = color;
    breathWaveContext.shadowBlur = 8+energy*14;
    breathWaveContext.stroke();
  });
  breathWaveContext.globalAlpha = 1;
  breathWaveContext.shadowBlur = 0;
}

let nativePresentationVisible = true;
window.fengyinSetPresentationVisible = visible => {
  nativePresentationVisible = !!visible;
  document.body.classList.toggle('presentation-hidden', !nativePresentationVisible);
  if(!nativePresentationVisible)hideNativeVideoViewport();
  updateVideoPresentation();
  if (!nativePresentationVisible) { clearTimeout(visualTimer); visualTimer=null; cancelAnimationFrame(animationFrame); }
  else requestVisualFrame();
};
window.__JUCE__?.backend?.addEventListener('presentationVisibility', window.fengyinSetPresentationVisible);
function requestVisualFrame() {
  if (visualTimer !== null || document.hidden || !nativePresentationVisible) return;
  const fps = document.body.classList.contains('realtime-rendering') ? 12 : 24;
  visualTimer = setTimeout(()=>{animationFrame=requestAnimationFrame(draw);},Math.max(0,1000/fps-(performance.now()-lastDrawAt)));
}
function updateStaticTheme() {
  const palettes = {spring:['#123c2b','#68e7ac'],summer:['#103940','#65e0d2'],autumn:['#38271c','#e7aa5e'],winter:['#183454','#b7ecff'],'china-red':['#431316','#ff4b52'],gold:['#392719','#ffc85a'],minimal:['#202b36','#75a9dc']};
  const [base,accent] = palettes[document.body.dataset.theme] || ['#112d49','#43d9ff'];
  const edge=getComputedStyle(document.body).getPropertyValue('--bg').trim();
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1600 1000"><defs><radialGradient id="g"><stop stop-color="${base}"/><stop offset="1" stop-color="${edge}"/></radialGradient></defs><rect width="1600" height="1000" fill="url(#g)"/><path d="M0 850 Q600 500 1600 120 M0 930 Q800 620 1600 280" fill="none" stroke="${accent}" stroke-opacity=".12" stroke-width="3"/></svg>`;
  document.body.style.backgroundImage = `url("data:image/svg+xml,${encodeURIComponent(svg)}")`;
}
function draw(timestamp = performance.now()) {
  visualTimer = null;
  if (document.hidden || !nativePresentationVisible || !$('#page-play').classList.contains('active') || !$('#technique-dialog').hidden) return;
  const realtime = document.body.classList.contains('realtime-rendering');
  const targetInterval = realtime ? 1000/12 : 1000/24;
  if (timestamp - lastDrawAt < targetInterval) {
    requestVisualFrame();
    return;
  }
  lastDrawAt = timestamp;
  if (!appFocused && ++unfocusedFrame % 4 !== 0) {
    return;
  }
  const ratio = window.devicePixelRatio || 1;
  const bounds = canvas.getBoundingClientRect();
  if(canvas.width !== Math.round(bounds.width*ratio) || canvas.height !== Math.round(bounds.height*ratio)) {
    canvas.width = Math.round(bounds.width*ratio); canvas.height = Math.round(bounds.height*ratio);
  }
  ctx.setTransform(ratio,0,0,ratio,0,0);
  ctx.clearRect(0,0,bounds.width,bounds.height);
  tick += simulating ? .045 : .008;
  const breath = hardwareBreath ?? 0;
  const safeBreath = Math.max(0,Math.min(100,breath));
  const rimTarget = safeBreath / 100;
  compactRimBreath += (rimTarget - compactRimBreath) * (rimTarget > compactRimBreath ? .12 : .055);
  if (!realtime) {
    document.body.style.setProperty('--breath-energy', compactRimBreath.toFixed(3));
    document.body.style.setProperty('--breath-speed', `${12-compactRimBreath*6}s`);
  }
  const tonePeak = currentPluginLoaded ? Math.min(1, Math.max(0, Number(latestBackendState.instrumentPeak || 0))) : 0;
  const styles = getComputedStyle(document.body);
  const a = styles.getPropertyValue('--accent').trim();
  const b = styles.getPropertyValue('--accent-2').trim();
  if (!realtime && $('#page-play').classList.contains('active')) drawBreathWave(ratio,safeBreath,a,b);
  const bars = compactMode ? 0 : 66;
  for(let i=0;i<bars;i++) {
    const x = i*bounds.width/bars;
    const wave = Math.abs(Math.sin(i*.24+tick*3)+Math.sin(i*.08-tick)*.45);
    const h = 7 + wave*safeBreath*.75*(.45+Math.sin(i/bars*Math.PI)*.55);
    const gradient = ctx.createLinearGradient(0,bounds.height-h,0,bounds.height);
    gradient.addColorStop(0,a); gradient.addColorStop(1,b);
    ctx.fillStyle = gradient;
    ctx.globalAlpha = .35 + wave*.55;
    ctx.fillRect(x,bounds.height-h,Math.max(3,bounds.width/bars-5),h);
  }
  ctx.globalAlpha = 1;
  setTextIfChanged($('#breath-value'),currentPluginLoaded ? `${Math.round(safeBreath)}%` : '—');
  $('#breath-bar').style.width = currentPluginLoaded ? `${safeBreath}%` : '0%';
  $('#meter-l').style.width = `${Math.min(100,Number(latestBackendState.outputLeftPeak||0)*100)}%`;
  $('#meter-r').style.width = `${Math.min(100,Number(latestBackendState.outputRightPeak||0)*100)}%`;
  if (!window.__JUCE__?.backend?.emitEvent) $('#cpu').textContent = `${Math.round(14+Math.abs(Math.sin(tick*.3))*9)}%`;
  // No recurring frame loop: new MIDI/meter state, resize or transition wakes us.
  if (!realtime && Math.abs(rimTarget-compactRimBreath)>.001) requestVisualFrame(); // Finish only the active fade.
}
updateStaticTheme(); requestVisualFrame();
window.addEventListener('resize', requestVisualFrame);
document.addEventListener('visibilitychange',()=>{updateVideoPresentation();if(document.hidden){clearTimeout(visualTimer);visualTimer=null;cancelAnimationFrame(animationFrame);}else requestVisualFrame();});

window.addEventListener('focus', () => { appFocused = true; unfocusedFrame = 0; requestVisualFrame(); });
window.addEventListener('blur', () => { appFocused = false; });

window.addEventListener('beforeunload', () => {
  cancelAnimationFrame(animationFrame);
  if(videoUrl) URL.revokeObjectURL(videoUrl);
});

// Tuner workspace uses native authorisation and native save acknowledgements.
// Keeping this separate from performance controls prevents draft state from
// becoming a public style merely because a tuner previews it.
const studioCss=document.createElement('style');
studioCss.textContent=`
.nav-item[data-page="chain"],#page-sounds .library-tabs,#page-sounds .library-context{display:none!important}
#page-sounds .sound-scan-card{grid-column:1/-1}
#page-sounds .instrument-preset-card.compact{grid-template-columns:minmax(0,1fr) 80px;min-height:150px}
#page-sounds .preset-card-actions{justify-content:center}
.studio-toolbar,.studio-names,.studio-fx-add,.studio-fx-row,.studio-draft-row{display:flex;gap:12px;align-items:center;flex-wrap:wrap}
.studio-toolbar{justify-content:space-between;margin-bottom:22px}.studio-names{margin-top:20px}
.studio-names label{flex:1;min-width:180px;color:var(--muted)}
.studio-names input,.studio-login input{display:block;width:100%;box-sizing:border-box;margin-top:8px;padding:12px;border:1px solid var(--line);border-radius:12px;background:#102238;color:var(--text)}
.studio-fx-row,.studio-draft-row{padding:14px;margin:10px 0;border:1px solid var(--line);border-radius:14px;background:#13283b}
.studio-fx-row strong,.studio-draft-row strong{flex:1;min-width:140px}.studio-fx-row small{color:var(--muted)}
.studio-fx-row button{padding:8px 12px}.studio-fx-row.is-bypassed{opacity:.55}
.studio-fx-add select{flex:1;min-width:170px;background:#102238;color:var(--text);padding:12px;border-radius:12px;border:1px solid var(--line)}
.studio-login{position:fixed;inset:0;z-index:10000;background:#020915bb;display:grid;place-items:center}
.studio-login[hidden]{display:none}.studio-login>div{width:min(400px,calc(100vw - 60px));padding:28px;background:#102238;border:1px solid var(--line);border-radius:20px}
.studio-login footer{display:flex;justify-content:flex-end;gap:12px;margin-top:24px}.studio-login p{color:var(--muted)}
.studio-login footer button{appearance:none;box-sizing:border-box;min-width:96px;min-height:44px;padding:10px 24px;border:1px solid var(--line);border-radius:12px;background:var(--panel);color:var(--text);font:inherit;font-weight:600;cursor:pointer}
.studio-login footer button.primary-button{background:linear-gradient(135deg,var(--accent),var(--accent-2));border-color:transparent;color:white}
.studio-login footer button:hover{filter:brightness(1.08)}
.studio-login footer button:focus-visible{outline:2px solid var(--accent);outline-offset:3px}
#studio-drafts:empty::before{content:'暂无草稿';color:var(--muted)}
.studio-final-label{margin:28px 0 12px;color:var(--accent,#43d9ff)}
`;
document.head.append(studioCss);
$('#advanced-studio').insertAdjacentHTML('beforeend',`<div class="tone-diagnostic-section"><button id="studio-entry" type="button">调音师入口</button><span id="studio-session-status" role="status"></span></div>`);
document.body.insertAdjacentHTML('beforeend',`<div class="studio-login" id="studio-login" hidden role="dialog" aria-modal="true" aria-labelledby="studio-login-title"><div><h2 id="studio-login-title">调音师入口</h2><label>密码<input id="studio-password" type="password" autocomplete="off"></label><p id="studio-login-status" role="status"></p><footer><button id="studio-login-cancel">取消</button><button id="studio-login-confirm" class="primary-button">进入</button></footer></div></div>`);
$('#page-chain .panel').insertAdjacentHTML('afterbegin',`<div class="studio-toolbar"><div><button id="studio-new">新建音色</button> <button id="studio-import">导入方案包</button> <button id="studio-show-drafts">草稿箱</button></div><button id="studio-exit">退出调音师</button></div><p id="studio-import-status" role="status" hidden></p><div id="studio-drafts" hidden></div>`);
$('#custom-tone-editor .custom-tone-step').insertAdjacentHTML('beforeend',`<div class="studio-names"><input id="studio-instrument-name" type="hidden"><label>音色名称<input id="studio-style-name" placeholder="例如：次中萨-醇厚" aria-describedby="tone-name-limit"><small id="tone-name-limit">最多10个字（含字母、数字、标点）</small></label></div>`);
$('#custom-effects-step').insertAdjacentHTML('afterbegin',`<div id="studio-effects"><h3>第三方效果器</h3><div class="studio-fx-add" id="studio-fx-add"></div><div id="studio-fx-chain"></div><p class="studio-final-label">最后 · 风吟内置效果</p></div>`);
document.querySelector('#custom-effects-step > h3').after($('#studio-effects'));
document.querySelector('#custom-effects-step .expert-inline h3')?.setAttribute('hidden','');
$('#effect-select').hidden=false; $('#load-effect').hidden=false; $('#load-effect').textContent='添加效果器';
$('#studio-fx-add').append($('#effect-select'),$('#load-effect'));
$('#save-expert').insertAdjacentHTML('afterend','<button id="studio-export" type="button">导出方案包</button>');
$('#start-custom-tone').textContent='创建调音草稿';
$('#studio-entry').onclick=()=>studioUnlocked?showPage('chain'):openStudioLogin();
$('#studio-style-name').maxLength=10;
function openStudioLogin(){
  const dialog=$('#studio-login'); if(!dialog)return;
  dialog.hidden=false; $('#studio-password').value=''; $('#studio-login-status').textContent=''; $('#studio-password').focus();
}
$('#studio-login-cancel').onclick=()=>{$('#studio-login').hidden=true;$('#studio-entry').focus();};
$('#studio-login-confirm').onclick=()=>{
  if(!window.__JUCE__?.backend?.emitEvent) return $('#studio-login-status').textContent='请在风吟软件中使用调音师功能';
  nativeEvent('studioLogin',{password:$('#studio-password').value});
  $('#studio-password').value='';
};
$('#studio-password').onkeydown=e=>{if(e.key==='Enter')$('#studio-login-confirm').click();};
window.__JUCE__?.backend?.addEventListener('studioAuthResult',ok=>{
  studioUnlocked=!!ok;
  if(!ok){$('#studio-login-status').textContent='密码不正确，或软件尚未激活';return;}
  $('#studio-login').hidden=true; showPage('chain');
});
$('#studio-new').onclick=()=>{
  if(expertDirty&&!window.confirm('放弃未保存的效果参数修改？'))return;
  $('#studio-drafts').hidden=true; enterCustomToneCreate();
};
$('#studio-show-drafts').onclick=()=>{
  $('#custom-tone-empty').hidden=true;$('#custom-tone-editor').hidden=true;$('#studio-drafts').hidden=false;renderStudioDrafts();
};
$('#studio-import').onclick=()=>{
  if(!studioUnlocked)return;
  if(expertDirty&&!window.confirm('导入会切换到新的调音草稿，放弃当前未保存的修改？'))return;
  nativeEvent('studioImport');
};
window.__JUCE__?.backend?.addEventListener('studioImportResult',result=>{
  const status=$('#studio-import-status');status.hidden=false;status.textContent=result?.message||'导入失败';
  if(!result?.success)return;
  expertDirty=false;studioSavedId=result.id;
  if(!result.loaded){$('#studio-show-drafts').click();return;}
  $('#studio-instrument-name').value=result.instrumentName||'';
  $('#studio-style-name').value=result.name||'';
  $('#studio-drafts').hidden=true;
  enterCustomToneEdit(result.name);showPage('chain');
});
$('#studio-exit').onclick=()=>{
  if(latestBackendState.pluginLoading||latestBackendState.effectLoading)return toast('插件正在加载，请稍候');
  if(expertDirty&&!window.confirm('退出并放弃未保存的修改？'))return;
  nativeEvent('studioLogout');studioUnlocked=false;expertDirty=false;customToneMode='idle';showPage('settings');
};
function saveStudioDraft(exportAfter, publish=false){
  if(!studioUnlocked||!currentPluginLoaded||!customSourceReady)return toast('请先加载并设置一个乐器');
  const name=$('#studio-style-name').value.trim(), instrumentName=$('#studio-instrument-name').value.trim();
  if(!name)return toast('请填写音色名称');
  if([...name].length>10)return toast('音色名称最多10个字，请缩短后保存');
  const duplicate=savedPresets.find(p=>p.name===name&&p.id!==studioSavedId);
  if(duplicate&&!window.confirm('已有同名方案，确定覆盖？取消后可修改名称另存。'))return;
  if(latestBackendState.pluginLoading||latestBackendState.effectLoading)return toast('请等待加载完成');
  studioExportAfterSave=exportAfter; $('#save-expert').disabled=true;$('#studio-export').disabled=true;$('#studio-publish').disabled=true;
  nativeEvent('saveCustomPreset',{name,instrumentName,publish,overwriteId:duplicate?.id||'',settings:expertSettings||toneStyleToExpert(),baseStyleId:'natural'});
}
$('#studio-export').onclick=()=>saveStudioDraft(true);
$('#studio-export').insertAdjacentHTML('afterend','<button id="studio-publish" type="button">添加到音色方案</button>');
$('#studio-publish').onclick=()=>saveStudioDraft(false,true);
window.__JUCE__?.backend?.addEventListener('studioSaveResult',id=>{
  $('#save-expert').disabled=false;$('#studio-export').disabled=false;$('#studio-publish').disabled=false;
  if(!id){studioExportAfterSave=false;return toast('草稿保存失败，请检查磁盘空间');}
  studioSavedId=String(id);editingPresetName=$('#studio-style-name').value.trim();expertDirty=false;
  toast('音色已保存');if(studioExportAfterSave)nativeEvent('studioExport',{id:studioSavedId});studioExportAfterSave=false;
});
window.__JUCE__?.backend?.addEventListener('studioExportResult',message=>toast(String(message)));
function renderStudioDrafts(){
  const root=$('#studio-drafts');if(!root)return;
  root.innerHTML=savedPresets.map((preset,index)=>({preset,index})).filter(({preset})=>preset.customTone).map(({preset,index})=>
    `<div class="studio-draft-row"><strong>${escapeHtml(preset.instrumentChineseName)} · ${escapeHtml(preset.name)}</strong><button data-studio-edit="${index}">继续调音</button><button data-studio-export="${index}">导出</button><button data-studio-delete="${index}">删除</button></div>`).join('');
  root.querySelectorAll('[data-studio-edit]').forEach(button=>button.onclick=()=>{
    const index=Number(button.dataset.studioEdit),preset=savedPresets[index];
    $('#studio-instrument-name').value=preset.instrumentChineseName;$('#studio-style-name').value=preset.name;
    studioSavedId=preset.id;pendingPresetNavigation={kind:'edit',name:preset.name};nativeEvent('editPreset',{index});root.hidden=true;
  });
  root.querySelectorAll('[data-studio-export]').forEach(button=>button.onclick=()=>nativeEvent('studioExport',{id:savedPresets[Number(button.dataset.studioExport)].id}));
  root.querySelectorAll('[data-studio-delete]').forEach(button=>button.onclick=()=>nativeEvent('deletePreset',{index:Number(button.dataset.studioDelete)}));
}
let studioChainSignature='',studioDraftSignature='';
window.__JUCE__?.backend?.addEventListener('backendState',state=>{
  studioUnlocked=!!state.studioUnlocked;
  $('#studio-import').disabled=!!state.pluginLoading||!!state.effectLoading;
  if(customToneMode==='edit'&&state.pluginLoaded&&!state.pluginLoading){
    const sourceIndex=availableInstruments.findIndex(item=>state.pluginIdentifier
      ?item.pluginId===state.pluginIdentifier:item.name===state.pluginName);
    if(sourceIndex>=0)$('#instrument-select').value=String(sourceIndex);
  }
  const rows=state.effectChain||[],signature=JSON.stringify(rows);
  if(signature!==studioChainSignature){
    studioChainSignature=signature;
    $('#studio-fx-chain').innerHTML=rows.map((row,index)=>`<div class="studio-fx-row ${row.bypassed?'is-bypassed':''}"><span>${index+1}</span><strong>${escapeHtml(row.name)} <small>${escapeHtml(row.version||'')}</small></strong><button data-effect-action="open" data-index="${index}">打开界面</button><button data-effect-action="bypass" data-index="${index}">${row.bypassed?'启用':'旁路'}</button><button data-effect-action="up" data-index="${index}" ${index===0?'disabled':''}>↑</button><button data-effect-action="down" data-index="${index}" ${index===rows.length-1?'disabled':''}>↓</button><button data-effect-action="remove" data-index="${index}">移除</button></div>`).join('');
    $$('#studio-fx-chain [data-effect-action]').forEach(button=>button.onclick=()=>{
      const index=Number(button.dataset.index);expertDirty=true;
      nativeEvent('studioEffectAction',{index,action:button.dataset.effectAction,bypassed:!rows[index].bypassed});
    });
  }
  const drafts=JSON.stringify(state.presets||[]);if(drafts!==studioDraftSignature){studioDraftSignature=drafts;renderStudioDrafts();}
  if(customSourceReady&&!$('#studio-instrument-name').value)$('#studio-instrument-name').value=currentInstrument?.chineseName||'';
});

// Bluetooth is a control-plane dialog. Meter updates never rebuild this device list.
document.body.insertAdjacentHTML('beforeend', `<div id="midi-connection-dialog" class="dialog-backdrop" hidden><section class="technique-dialog midi-dialog" role="dialog" aria-modal="true" aria-labelledby="midi-connection-title"><h2 id="midi-connection-title">连接电吹管</h2><p>请插入 USB 连接线，或通过蓝牙连接电吹管。</p><div class="midi-toolbar"><b>蓝牙 MIDI 设备</b><button id="midi-search" type="button">搜索蓝牙设备</button></div><p id="midi-connection-message" role="status"></p><div id="midi-device-list" class="midi-device-list"></div><div id="midi-usb-list" class="midi-device-list"></div><div id="midi-confirm" class="midi-confirm" hidden><p id="midi-confirm-text"></p><footer><button id="midi-confirm-cancel" type="button">取消</button><button id="midi-confirm-ok" class="primary-button" type="button">确认</button></footer></div><div class="dialog-actions"><button id="midi-adapt" type="button">智能适配</button><button id="midi-connection-close" class="primary-button" type="button">完成</button></div></section></div>`);
let midiConnectionState = {supported:false,devices:[],usbDevices:[]}, midiDialogReturnFocus=null, midiConfirmation=null;
function midiCommand(action,id=''){nativeEvent('midiConnection',{action,id});}
function openMidiConnection(){
  midiDialogReturnFocus=document.activeElement;
  $('#midi-connection-dialog').hidden=false;
  midiCommand('state'); renderMidiConnection(midiConnectionState); $('#midi-search').focus();
}
function closeMidiConnection(){
  midiCommand('stopScan');$('#midi-connection-dialog').hidden=true;
  $('#midi-confirm').hidden=true;midiConfirmation=null;midiDialogReturnFocus?.focus();
}
function confirmMidiAction(text,action){
  $('#midi-confirm-text').textContent=text;$('#midi-confirm').hidden=false;
  midiConfirmation=action;$('#midi-confirm-cancel').focus();
}
function midiDeviceDisambiguator(id){
  // Windows IDs often end with the same interface GUID; a suffix cannot identify a device.
  let hash=2166136261;for(const character of String(id)){hash^=character.codePointAt(0);hash=Math.imul(hash,16777619);}
  return (hash>>>0).toString(16).padStart(8,'0').toUpperCase();
}
function renderMidiConnection(state){
  midiConnectionState=state;
  const busy=!!state.connectingId;
  $('#midi-search').disabled=!state.supported||busy;
  $('#midi-search').textContent=state.scanning?'停止搜索':'搜索蓝牙设备';
  $('#midi-connection-message').textContent=state.message||'点击搜索，查找附近支持蓝牙 MIDI 的电吹管。';
  const signature=JSON.stringify([state.devices,state.connectedId,state.connectingId,state.inputConnected,state.inputName,state.inputTransport,state.inputId,state.usbDevices]);
  if($('#midi-device-list').dataset.signature!==signature){
    $('#midi-device-list').dataset.signature=signature;
    const list=$('#midi-device-list');list.replaceChildren();
    const rows=[...(state.devices||[])].sort((a,b)=>Number(b.id===state.connectedId)-Number(a.id===state.connectedId)||Number(b.available)-Number(a.available)||String(a.name).localeCompare(String(b.name)));
    for(const row of rows){
      const connected=row.id===state.connectedId,connecting=row.id===state.connectingId;
      const card=document.createElement('div');card.className='midi-device'+(connected?' connected':!row.available?' unavailable':'');
      const label=document.createElement('div');label.className='device-label';
      const name=document.createElement('b');name.title=row.id;name.textContent=(row.name||'蓝牙 MIDI 电吹管')+((state.devices||[]).filter(d=>d.name===row.name).length>1?` · ${midiDeviceDisambiguator(row.id)}`:'');
      const status=document.createElement('small');status.textContent=connected?'已连接 · 蓝牙 MIDI':connecting?'正在连接…':row.available?'已发现 · 可连接':'历史设备 · 当前未发现';
      label.append(name,status);card.append(label);
      const button=document.createElement('button');button.type='button';button.textContent=connected?'断开':connecting?'取消连接':'连接';
      button.disabled=(!connected&&!connecting&&!row.available)||(busy&&!connecting);
      button.onclick=()=>{
        if(connected||connecting){midiCommand(connecting?'cancel':'disconnect');return;}
        const connect=()=>midiCommand('connect',row.id);
        if(state.inputConnected)confirmMidiAction('连接此设备将切换当前演奏输入，不会同时接收两台设备。',connect);else connect();
      };card.append(button);
      if(row.remembered||row.paired){const forget=document.createElement('button');forget.type='button';forget.textContent='忘记设备';forget.disabled=busy;
        forget.onclick=()=>confirmMidiAction(`忘记“${row.name||'此设备'}”？将清除连接记录及系统配对${connected?'，并断开当前连接':''}。下次使用需重新连接。`,()=>midiCommand('forget',row.id));card.append(forget);}
      list.append(card);
    }
    if(!rows.length){const empty=document.createElement('p');empty.textContent=state.scanning?'正在查找附近设备…':'尚无蓝牙 MIDI 设备。请开启电吹管蓝牙后搜索。';list.append(empty);}
    const usb=$('#midi-usb-list');usb.replaceChildren();
    for(const row of state.usbDevices||[]){const card=document.createElement('div');card.className='midi-device';const label=document.createElement('div');label.className='device-label';const name=document.createElement('b');name.textContent=row.name;const hint=document.createElement('small');hint.textContent='有线 / 系统 MIDI 输入';label.append(name,hint);const button=document.createElement('button');button.type='button';const connected=state.inputConnected&&state.inputTransport==='usb'&&state.inputId===row.id;button.textContent=connected?'断开':'使用此设备';button.disabled=busy;button.onclick=()=>{if(connected){midiCommand('disconnect');return;}const connect=()=>midiCommand('usb',row.id);if(state.inputConnected)confirmMidiAction('切换到此 MIDI 输入？当前演奏连接将断开。',connect);else connect();};card.append(label,button);usb.append(card);}
  }
  const status=state.inputConnected?`${state.inputTransport==='bluetooth'?'蓝牙':'有线'} · ${state.inputName||'电吹管'}`:'连接电吹管';
  $('#compact-midi-connect').textContent=state.inputConnected?'管理连接':'连接电吹管';$('#compact-midi-connect').title=status;
  $('#wind-status-pill').title=status;
  $('.sidebar-status').setAttribute('aria-label',state.inputConnected?`管理连接：${status}`:'点击连接电吹管');
}
$('.sidebar-status')?.addEventListener('click',openMidiConnection);
$('.sidebar-status')?.addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();openMidiConnection();}});
$('#compact-midi-connect')?.addEventListener('click',openMidiConnection);
$('#midi-search').onclick=()=>midiCommand(midiConnectionState.scanning?'stopScan':'scan');
$('#midi-connection-close').onclick=closeMidiConnection;
$('#midi-adapt').onclick=()=>{closeMidiConnection();showPage('wind');};
$('#midi-confirm-cancel').onclick=()=>{$('#midi-confirm').hidden=true;midiConfirmation=null;};
$('#midi-confirm-ok').onclick=()=>{const action=midiConfirmation;midiConfirmation=null;$('#midi-confirm').hidden=true;action?.();};
$('#midi-connection-dialog').addEventListener('keydown',e=>{
  if(e.key==='Escape'){e.stopPropagation();closeMidiConnection();}
  if(e.key==='Tab'){const controls=[...e.currentTarget.querySelectorAll('button:not([disabled])')].filter(e=>e.offsetParent!==null);const first=controls[0],last=controls.at(-1);if(e.shiftKey&&document.activeElement===first){e.preventDefault();last?.focus();}else if(!e.shiftKey&&document.activeElement===last){e.preventDefault();first?.focus();}}
});
window.__JUCE__?.backend?.addEventListener('midiConnectionState',renderMidiConnection);
