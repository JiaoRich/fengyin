const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => [...document.querySelectorAll(selector)];

const titles = {play:'开始演奏',sounds:'音色方案',chain:'定制音色',wind:'智能适配',audio:'声音设置',settings:'软件设置'};
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
  ['小提琴','SWAM Violin','自然独奏、温暖抒情、电影叙事']
  ,['中提琴','SWAM Viola','自然独奏、温暖抒情、室内乐叙事']
  ,['大提琴','SWAM Cello','自然独奏、深情抒情、电影史诗']
  ,['低音提琴','SWAM Double Bass','自然独奏、温暖低音、电影厚重']
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

let simulating = true;
let recording = false;
let animationFrame;
let videoUrl;
let selectedVideoFile;
let videoGeneration = 0;
let videoLoading = false;
let videoWantsPlaying = false;
let videoPlayRequest = 0;
let triedVideoDataFallback = false;
let nativeAccompanimentReady = false;
let lastVideoSyncAt = 0;
let availableInstruments = [];
let availableEffects = [];
let pluginListSignature = '';
let savedPresets = [];
let presetListSignature = '';
let hardwareBreath = null;
let currentArtworkKey = '';
let currentInstrument = null;
let currentPluginLoaded = false;
let pendingPresetNavigation = null;
let customToneMode = 'idle';
let editingPresetName = '';
let techniqueMappings = [];
let techniquePlan = [];
let globalTechniqueMode = 'breath';
let latestBackendState = {};
let appFocused = document.hasFocus();
let unfocusedFrame = 0;
let selectedTargetKey = 0;
let techniqueSensitivity = 'standard';
let optimisationTimer = 0;
let lastDrawAt = 0;
let currentToneStyleIndex = 0;
let currentToneStyleInstrument = '';
let currentInstrumentModelIndex = 0;
let currentInstrumentModelKey = '';
let activeLibraryTab = 'swam';
let expertSettings = null;
let expertDirty = false;
let reverbDragging = false;

const toneStyleLibrary = {
  'soprano-sax': [
    {id:'natural',name:'自然原声',description:'真实、均衡、保留原始动态',eq:0,reverb:18,warmth:18},
    {id:'silky',name:'丝滑抒情',description:'温暖、柔和、浪漫大厅',eq:-12,reverb:36,warmth:58},
    {id:'stage',name:'明亮舞台',description:'清晰、明亮、更有穿透力',eq:28,reverb:24,warmth:30}
  ],
  'alto-sax': [
    {id:'natural',name:'自然原声',description:'均衡自然，适合多数曲目',eq:0,reverb:18,warmth:20},
    {id:'warm-jazz',name:'温暖爵士',description:'厚实、松弛、带轻微暖色',eq:-18,reverb:25,warmth:66},
    {id:'pop',name:'流行穿透',description:'结实明快，容易融入伴奏',eq:30,reverb:20,warmth:32}
  ],
  'tenor-sax': [
    {id:'natural',name:'自然原声',description:'宽厚自然，动态完整',eq:0,reverb:18,warmth:25},
    {id:'smoky',name:'烟熏爵士',description:'低沉、温暖、略带粗粝感',eq:-24,reverb:23,warmth:74},
    {id:'lyrical',name:'深情抒情',description:'圆润、舒展、柔和大厅',eq:-10,reverb:34,warmth:60}
  ],
  trumpet: [
    {id:'natural',name:'自然原声',description:'清晰真实，保持铜管动态',eq:0,reverb:17,warmth:16},
    {id:'bright-pop',name:'明亮流行',description:'有冲击力，适合舞台与流行',eq:32,reverb:18,warmth:28},
    {id:'soft',name:'柔和抒情',description:'收敛刺耳感，温暖耐听',eq:-20,reverb:32,warmth:52}
  ],
  flute: [
    {id:'natural',name:'自然原声',description:'自然气声与真实动态',eq:0,reverb:20,warmth:12},
    {id:'clear',name:'通透明亮',description:'清澈通透，适合轻快旋律',eq:25,reverb:22,warmth:10},
    {id:'airy',name:'空灵抒情',description:'气息感更强，空间更宽广',eq:-8,reverb:42,warmth:36}
  ],
  violin: [
    {id:'natural',name:'自然独奏',description:'真实弓感与自然空间',eq:0,reverb:22,warmth:18},
    {id:'warm',name:'温暖抒情',description:'柔和圆润，适合慢歌旋律',eq:-16,reverb:34,warmth:56},
    {id:'cinematic',name:'电影叙事',description:'宽广、明亮、具有画面感',eq:18,reverb:40,warmth:38}
  ],
  'low-brass': [
    {id:'natural',name:'自然原声',description:'保留铜管真实动态',eq:0,reverb:18,warmth:22},
    {id:'warm-orchestral',name:'温暖交响',description:'厚实圆润，容易融入乐团',eq:-18,reverb:27,warmth:58},
    {id:'cinematic-brass',name:'电影史诗',description:'宽广雄浑，富有力量',eq:15,reverb:38,warmth:34}
  ],
  woodwind: [
    {id:'natural',name:'自然原声',description:'保留木管的真实音头与动态',eq:0,reverb:18,warmth:20},
    {id:'warm-lyrical',name:'温暖抒情',description:'柔和圆润，适合歌唱性旋律',eq:-16,reverb:31,warmth:52},
    {id:'cinematic-wood',name:'电影叙事',description:'清晰宽广，富有画面感',eq:12,reverb:39,warmth:34}
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
  'violin':'instrument_violin.png', 'viola':'instrument_viola.png',
  'cello':'instrument_cello.png', 'double-bass':'instrument_double_bass.png'
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
  if (key.startsWith('kong-')) {
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
  return (techniqueLibrary[instrumentFamily(key)] || []).map(([id,name,icon,description,mode,recommendation,strength,featured], index) => ({
    id,name,icon,description,mode,recommendation,strength,featured,index
  }));
}

function buildAdaptiveTechniquePlan(key = '', state = {}) {
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
  const roles = [
    {id:'vibrato',name:'颤动控制',description:'萨克斯颤音、提琴揉弦、铜管唇颤音',icon:'≈',strength:55,index:1},
    {id:'growl',name:'质感控制',description:'萨克斯嘶吼、木管花舌、弦乐颤弓',icon:'≋',strength:50,index:0},
    {id:'portamento',name:'滑音控制',description:'根据当前乐器控制滑音与过渡',icon:'⌁',strength:45,index:3},
    {id:'mute',name:'特殊技巧',description:'弱音器、拨奏或替代指法',icon:'◇',strength:50,index:8}
  ];
  if (!window.__JUCE__?.backend?.emitEvent || !Array.isArray(mappings)) return roles.map(item=>({...item,mode:'breath',featured:true}));
  const modes = ['breath','breath','hardware','hardware','breath'];
  return roles.map(item => {
    const backend = mappings.find(entry => entry.id === item.id);
    if (!backend) return {...item,mode:'breath',featured:true};
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
      pluginSupported:true
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
    ['低音提琴|double bass','double-bass'],['小提琴|violin','violin'],['中提琴|viola','viola'],['大提琴|cello','cello']
  ];
  return rules.find(([pattern]) => new RegExp(pattern).test(text))?.[1] || 'alto-sax';
}

function isSupportedInstrument(item = {}) {
  if (item.supported === false) return false;
  return /swam|kong|qin/i.test(`${item.name || ''} ${item.label || ''} ${item.brand || ''}`)
    || item.isSwam === true || item.brand === 'kong';
}

function showInstrumentArtwork(key, chineseName = '') {
  const picture = $('#instrument-picture');
  if (key.startsWith('kong-')) {
    currentArtworkKey = key;
    picture.hidden = true;
    picture.removeAttribute('src');
    $('#instrument-art').classList.remove('empty');
    $('#instrument-art').classList.add('folk');
    $('#instrument-empty').querySelector('span').textContent = kongPresets.find(item => item[1] === key)?.[2] || '乐';
    $('#instrument-empty').querySelector('strong').textContent = chineseName;
    $('#instrument-empty').querySelector('small').textContent = '空音 Qin Engine V3 · 气息与技巧已适配';
    $('.breath-track').classList.remove('inactive');
    $('#technique-settings').disabled = false;
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
  $('#technique-settings').disabled = false;
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
  $('#technique-settings').disabled = true;
}

function applyPrototypeInstrument(key, chineseName, pluginName) {
  currentPluginLoaded = true;
  currentInstrument = {name:pluginName, chineseName, instrumentKey:key};
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
  if (window.__JUCE__?.backend?.emitEvent) window.__JUCE__.backend.emitEvent(name, payload);
}

function toast(message) {
  const el = $('#toast');
  el.textContent = message;
  el.classList.add('show');
  clearTimeout(el.timer);
  el.timer = setTimeout(() => el.classList.remove('show'), 2200);
}

function showPage(page) {
  $$('.nav-item,.page').forEach(el => el.classList.remove('active'));
  const button = $(`.nav-item[data-page="${page}"]`);
  button.classList.add('active');
  $(`#page-${page}`).classList.add('active');
  $('#page-title').textContent = titles[page];
  if (page === 'audio') nativeEvent('requestAudioSettings');
}

$$('.nav-item').forEach(button => button.addEventListener('click', () => showPage(button.dataset.page)));
$('#choose-sound-from-empty').addEventListener('click', () => showPage('sounds'));

function escapeHtml(value) {
  return String(value ?? '').replace(/[&<>"']/g, character => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[character]));
}

function toneStylesForInstrument(key = '') {
  if (!key) return [{id:'none',name:'请先加载音色',description:'加载乐器后即可选择音色风格',eq:0,reverb:18,warmth:20}];
  if (key.startsWith('kong-')) {
    const folkName = currentInstrument?.chineseName || '民乐';
    const windLandscape = key.startsWith('kong-dizi') || ['kong-xiao','kong-nanxiao','kong-xun'].includes(key);
    const festiveWind = key.startsWith('kong-suona') || key === 'kong-guanzi';
    const profiles = windLandscape
      ? [['natural','自然原声',`保留${folkName}真实气息与音头`,0,20,24],['silk-bamboo','清雅丝竹','清透自然，保留气息细节',12,29,28],['landscape','空灵山水','宽广悠远，适合古风抒情',-10,44,42]]
      : festiveWind
        ? [['natural','自然原声',`保留${folkName}真实动态`,0,18,18],['festive','喜庆明亮','高亢清晰，适合舞台与喜庆曲目',24,20,18],['epic-folk','厚重叙事','收敛刺耳感，增加厚重空间',-8,35,46]]
        : [['natural','自然原声',`保留${folkName}真实质感`,0,20,24],['warm-folk','温暖抒情','柔和耐听，适合抒情曲目',-12,32,55],['cinematic-folk','国风叙事','清晰宽广，适合古风与影视',22,36,34]];
    return profiles
      .map(([id,name,description,eq,reverb,warmth]) => ({id,name,description,eq,reverb,warmth}));
  }
  const family = instrumentFamily(key);
  return toneStyleLibrary[key]
    || (family === 'saxophone' ? (key === 'baritone-sax' ? toneStyleLibrary['tenor-sax'] : toneStyleLibrary['alto-sax']) : null)
    || (family === 'flute' ? toneStyleLibrary.flute : null)
    || (family === 'woodwind' ? toneStyleLibrary.woodwind : null)
    || (family === 'strings' ? toneStyleLibrary.violin : null)
    || (family === 'brass' ? (key.includes('trombone') || key.includes('tuba') || key.startsWith('horn-') || key === 'euphonium' ? toneStyleLibrary['low-brass'] : toneStyleLibrary.trumpet) : null)
    || [{id:'natural',name:'自然原声',description:'保留乐器原始音色与动态',eq:0,reverb:20,warmth:20}];
}

function toneVariantsForCurrentInstrument() {
  if (!currentInstrument) return toneStylesForInstrument('');
  const builtins = toneStylesForInstrument(currentInstrument.instrumentKey).map(style=>({...style,custom:false}));
  const customs = savedPresets.map((preset,index)=>({preset,index}))
    .filter(({preset})=>preset.instrumentKey===currentInstrument.instrumentKey)
    .map(({preset,index})=>({id:`custom:${preset.id}`,name:preset.name,description:'我的音色方案',custom:true,presetIndex:index,
      eq:0,reverb:Math.round(Number(latestBackendState.reverbMix||.2)*100),warmth:20}));
  return [...builtins,...customs];
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
  selector.innerHTML = styles.map(style => `<option value="${style.id}">${style.name}</option>`).join('');
  selector.disabled = !enabled;
  $('#tone-style-prev').disabled = !enabled || styles.length < 2;
  $('#tone-style-next').disabled = !enabled || styles.length < 2;
  $('#tone-style-switcher').classList.toggle('disabled', !enabled);
  if ($('#chain-style-select')) {
    $('#chain-style-select').innerHTML = selector.innerHTML;
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
  const styles = toneStylesForInstrument(currentInstrument?.instrumentKey);
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
  const enabled = !!currentPluginLoaded && models.length > 0;
  const backendName = latestBackendState?.instrumentModelName;
  const backendIndex = models.findIndex(model => model.name === backendName || model.id === String(latestBackendState?.instrumentModelId ?? ''));
  if (reset || currentInstrumentModelKey !== key) {
    currentInstrumentModelKey = key;
    currentInstrumentModelIndex = backendIndex >= 0 ? backendIndex : Math.max(0, models.findIndex(model => model.name === currentInstrument?.name));
  }
  else if (backendIndex >= 0) currentInstrumentModelIndex = backendIndex;
  currentInstrumentModelIndex = Math.max(0, Math.min(models.length - 1, currentInstrumentModelIndex));
  $('#instrument-model-name').textContent = enabled ? models[currentInstrumentModelIndex].name
    : currentPluginLoaded ? '当前音源未开放型号' : '尚未加载乐器';
  $('#instrument-model-prev').disabled = !enabled || models.length < 2;
  $('#instrument-model-next').disabled = !enabled || models.length < 2;
  $('#instrument-model-switcher').classList.toggle('disabled', !enabled);
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
  ['warmth','温暖度 · Warmth',20],['saturation','饱和度 · Saturation',12],
  ['compression','压缩阈值 · Threshold',35],['ratio','压缩比 · Ratio',25],
  ['harsh','去刺耳 · Harsh Control',25],['reverb','混响强度 · Reverb Mix',18],
  ['room','空间大小 · Room Size',42],['damping','高频阻尼 · Damping',54],
  ['width','立体声宽度 · Width',88],['output','输出音量 · Output Gain',80]
];
function toneStyleToExpert() {
  const style = toneStylesForInstrument(currentInstrument?.instrumentKey)[currentToneStyleIndex] || {};
  if (latestBackendState?.toneSettings && window.__JUCE__?.backend?.emitEvent)
    return Object.fromEntries(expertControls.map(([id,,fallback]) => [id,Number(latestBackendState.toneSettings[id] ?? fallback)]));
  return {bass:50,brightness:Math.round((Number(style.eq || 0)+100)/2),warmth:Number(style.warmth ?? 20),compression:35,harsh:25,reverb:Number(style.reverb ?? 18),room:42,output:80};
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
  if (id === 'output') return `${(20*Math.log10(.5+value/100*.75)).toFixed(1)} dB`;
  return `${value.toFixed(1)}%`;
}
// The custom-tone editor lives directly in the second step.
$('#custom-effects-step').append(document.querySelector('#expert-dialog .expert-inline'));
function enterCustomToneCreate() {
  customToneMode='create'; editingPresetName=''; expertDirty=false;
  $('#custom-tone-empty').hidden=true; $('#custom-tone-editor').hidden=false; $('#custom-effects-step').hidden=true;
  $('#instrument-select').disabled=false; $('#instrument-select').value=''; $('#open-instrument').disabled=true;
  $('#instrument-load-state').textContent='请选择一个乐器音源';
  $('#save-expert').textContent='保存方案'; $('#save-as-expert').hidden=true; $('#reset-expert').hidden=true;
  $('#cancel-expert').textContent='取消定制';
}
function enterCustomToneEdit(name) {
  customToneMode='edit'; editingPresetName=name; expertDirty=false;
  $('#custom-tone-empty').hidden=true; $('#custom-tone-editor').hidden=false; $('#custom-effects-step').hidden=false;
  $('#instrument-select').disabled=true; $('#open-instrument').disabled=false;
  $('#save-expert').textContent='保存修改'; $('#save-as-expert').hidden=true; $('#reset-expert').hidden=true;
  $('#cancel-expert').textContent='取消修改'; refreshInlineExpert();
}
$('#start-custom-tone').addEventListener('click', enterCustomToneCreate);
function refreshInlineExpert() {
  if (expertDirty || $('#expert-grid').contains(document.activeElement)) return;
  expertSettings = toneStyleToExpert();
  renderExpertControls();
  $('#save-expert').textContent = editingPresetName ? '保存修改' : '保存为我的方案';
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
  if (editingPresetName) {
    nativeEvent('saveCustomPreset', {name:editingPresetName,settings:expertSettings,baseStyleId:toneStylesForInstrument(currentInstrument?.instrumentKey)[currentToneStyleIndex]?.id || 'natural'});
    expertDirty=false; customToneMode='idle'; showPage('sounds');
    toast(`已保存对“${editingPresetName}”的修改`);
    return;
  }
  $('#preset-name-input').value = '';
  $('#preset-name-dialog').hidden = false;
  $('#preset-name-input').focus();
});
$('#cancel-preset-name')?.addEventListener('click', () => { $('#preset-name-dialog').hidden = true; });
$('#confirm-preset-name')?.addEventListener('click', () => {
  const name = $('#preset-name-input').value.trim();
  if (!name) return toast('请输入方案名称');
  const duplicate = savedPresets.some(item => String(item.name || '').toLowerCase() === name.toLowerCase()
    && (item.brand || 'swam') === (currentInstrument?.instrumentKey?.startsWith('kong-') ? 'kong' : 'swam'));
  if (duplicate && !window.confirm(`已有名为“${name}”的方案。\n\n点击“确定”覆盖，点击“取消”返回改名。`)) return;
  nativeEvent('saveCustomPreset', {name,settings:expertSettings,baseStyleId:toneStylesForInstrument(currentInstrument?.instrumentKey)[currentToneStyleIndex]?.id || 'natural'});
  if (!window.__JUCE__?.backend?.emitEvent) {
    savedPresets.unshift({name,brand:currentInstrument?.instrumentKey?.startsWith('kong-')?'kong':'swam',instrumentChineseName:currentInstrument?.chineseName});
    renderPresets();
  }
  $('#preset-name-dialog').hidden = true;
  expertDirty=false; customToneMode='idle'; showPage('sounds');
  toast(`已保存：${name}`);
});
renderToneStyleSwitcher();

function renderPresets() {
  const hasSwam = availableInstruments.some(item => item.brand === 'swam' || item.isSwam === true);
  const hasKong = availableInstruments.some(item => item.brand === 'kong' || /kong|qin/i.test(`${item.name || ''} ${item.label || ''}`));
  const availableLibraries = [hasSwam && 'swam',hasKong && 'kong'].filter(Boolean);
  if (!availableLibraries.includes(activeLibraryTab)) activeLibraryTab = availableLibraries[0] || 'swam';
  $$('.library-tab').forEach(tab => {
    const available = tab.dataset.library === 'swam' ? hasSwam : hasKong;
    tab.hidden = !available;
    const selected = available && tab.dataset.library === activeLibraryTab;
    tab.classList.toggle('active', selected);
    tab.setAttribute('aria-selected', String(selected));
  });
  $('#library-tabs').hidden = availableLibraries.length < 2;
  $('#library-context').hidden = availableLibraries.length === 0;
  if (!availableLibraries.length) {
    $('#preset-grid').innerHTML = '<button class="preset preset-create" data-kind="scan"><i aria-hidden="true">⌕</i><h3>扫描本机音源</h3><b>尚未找到可用音源</b><span>扫描后只显示已安装的音源和乐器</span></button>';
    document.querySelector('#preset-grid [data-kind="scan"]').addEventListener('click', () => nativeEvent('scanPlugins'));
    return;
  }
  updateLibraryContext();
  const installedKong = Array.isArray(latestBackendState.kongInstruments) ? latestBackendState.kongInstruments : [];
  const create = '<button class="preset preset-create" data-action="create"><i aria-hidden="true">＋</i><h3>定制我的音色</h3><b>选择音源并实时试听</b></button>';
  const swamCards = availableInstruments.map((instrument,pluginIndex)=>({instrument,pluginIndex}))
    .filter(({instrument}) => instrument.brand === 'swam' || instrument.isSwam === true)
    .map(({instrument,pluginIndex}) => ({key:instrument.instrumentKey || inferInstrumentKey(instrument.name),name:instrument.chineseName || instrument.name,pluginIndex,brand:'swam'}));
  const kongCards = installedKong.map((instrument,kongIndex)=>({key:instrument.key,name:instrument.name,program:instrument.program,kongIndex,brand:'kong'}));
  const cards = (activeLibraryTab === 'kong' ? kongCards : swamCards).filter((item,index,array)=>array.findIndex(other=>other.key===item.key)===index);
  const renderStyle = (style,card,isCustom,presetIndex=-1) => `<div class="tone-row ${latestBackendState.activeToneVariantId === (isCustom ? `custom:${savedPresets[presetIndex]?.id}` : `builtin:${card.key}:${style.id}`) ? 'active' : ''}"><div><strong>${escapeHtml(style.name)}</strong><small>${isCustom ? '我的方案' : '默认方案'}</small></div><div class="tone-row-actions"><button data-action="${isCustom?'perform-custom':'perform-builtin'}" data-preset-index="${presetIndex}" data-plugin-index="${card.pluginIndex ?? -1}" data-kong-index="${card.kongIndex ?? -1}" data-style-id="${escapeHtml(style.id)}">演奏</button><button data-action="${isCustom?'edit-custom':'locked'}" data-preset-index="${presetIndex}" aria-disabled="${!isCustom}">编辑</button><button data-action="${isCustom?'delete-custom':'locked'}" data-preset-index="${presetIndex}" aria-disabled="${!isCustom}">删除</button></div></div>`;
  const instrumentCards = cards.map(card => {
    const builtins = toneStylesForInstrument(card.key).map(style=>renderStyle(style,card,false)).join('');
    const customs = savedPresets.map((preset,index)=>({preset,index})).filter(({preset})=>preset.instrumentKey===card.key)
      .map(({preset,index})=>renderStyle({id:`custom:${preset.id}`,name:preset.name},card,true,index)).join('');
    return `<section class="instrument-preset-card"><header><div><span>${card.brand==='kong'?'中国民乐 · 空音':'西洋乐器 · SWAM'}</span><h3>${escapeHtml(card.name)}</h3></div></header><div class="tone-list">${builtins}${customs}</div></section>`;
  }).join('');
  $('#preset-grid').innerHTML = create + instrumentCards;
  $$('#preset-grid [data-action]').forEach(button => button.addEventListener('click', () => {
    const action = button.dataset.action;
    if (action === 'create') {
      editingPresetName=''; nativeEvent('cancelPresetEdit'); showPage('chain'); enterCustomToneCreate(); return;
    }
    if (action === 'locked') return toast('默认方案，不支持编辑或删除');
    const presetIndex = Number(button.dataset.presetIndex);
    if (action === 'perform-custom') {
      const preset=savedPresets[presetIndex]; pendingPresetNavigation={kind:'preset',name:preset?.name||'我的音色'};
      nativeEvent('loadPreset',{index:presetIndex}); return toast(`正在载入：${preset?.name||'我的音色'}`);
    }
    if (action === 'edit-custom') {
      const preset=savedPresets[presetIndex]; pendingPresetNavigation={kind:'edit',name:preset?.name||'我的音色'};
      nativeEvent('editPreset',{index:presetIndex}); return toast(`正在打开：${preset?.name||'我的音色'}`);
    }
    if (action === 'delete-custom') {
      const preset=savedPresets[presetIndex]; nativeEvent('deletePreset',{index:presetIndex}); return toast(`请确认是否删除“${preset?.name||'这个音色'}”`);
    }
    if (action === 'perform-builtin') {
      const styleId=button.dataset.styleId;
      pendingPresetNavigation={kind:'plugin',name:button.closest('.instrument-preset-card').querySelector('h3').textContent,styleId};
      if (Number(button.dataset.kongIndex)>=0) {
        const item=installedKong[Number(button.dataset.kongIndex)]; nativeEvent('loadKongInstrument',{instrumentKey:item.key,instrumentName:item.name});
      } else nativeEvent('loadPlugin',{index:Number(button.dataset.pluginIndex)});
      return toast('正在载入音色…');
    }
  }));
}
renderPresets();

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
}));
updateLibraryContext();
const previewParams = new URLSearchParams(window.location.search);
if (previewParams.get('library') === 'kong') $('.library-tab[data-library="kong"]')?.click();
if (previewParams.get('preview') === 'sounds') showPage('sounds');

const video = $('#video');
const videoFileInput = $('#video-file');
function chooseVideoFile() {
  videoWantsPlaying = false;
  video.pause();
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
  lastVideoSyncAt = 0;
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
  video.src = videoUrl;
  video.load();
  $('#video-name').textContent = file.name;
  $('#change-video').textContent = '更换视频';
  event.target.value = '';
  toast('正在载入视频…');
});
video.addEventListener('loadedmetadata', () => {
  videoLoading = false;
  video.currentTime = 0;
  video.style.display = 'block';
  $('#video-empty').style.display = 'none';
  $('#duration').textContent = formatTime(video.duration);
  toast('视频已载入，可以播放');
});
video.addEventListener('error', () => {
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
  $('#video-name').textContent = '视频编码不受支持，请转换为 H.264 + AAC';
});

function toggleVideo() {
  if (!video.src) return toast('请先选择一个本地视频');
  if (videoLoading || (window.__JUCE__?.backend?.emitEvent && !nativeAccompanimentReady))
    return toast('视频正在准备，请稍候');
  videoWantsPlaying = !videoWantsPlaying;
  const request = ++videoPlayRequest;
  if (!videoWantsPlaying) {
    video.pause();
    nativeEvent('setVideoPlaybackState',{generation:videoGeneration,playing:false,position:video.currentTime});
  } else video.play().catch(error => {
    if (request !== videoPlayRequest) return;
    videoWantsPlaying = false;
    if (error.name !== 'AbortError') toast('暂时无法播放，请重新选择视频');
  });
}
$('#play-button').addEventListener('click', toggleVideo);
$('#main-play').addEventListener('click', toggleVideo);
video.addEventListener('playing', () => { if (videoLoading || !videoWantsPlaying) { video.pause(); return; } document.body.classList.add('video-playing'); nativeEvent('setVideoPlaybackState',{generation:videoGeneration,playing:true,position:video.currentTime}); $('#play-button').textContent='Ⅱ'; $('#main-play').textContent='Ⅱ 暂停视频'; });
video.addEventListener('pause', () => { document.body.classList.remove('video-playing'); if (!videoLoading) nativeEvent('setVideoPlaybackState',{generation:videoGeneration,playing:false,position:video.currentTime}); $('#play-button').textContent='▶'; $('#main-play').textContent='▶ 播放视频'; });
video.addEventListener('ended', () => { videoWantsPlaying = false; document.body.classList.remove('video-playing'); nativeEvent('setVideoPlaybackState',{generation:videoGeneration,playing:false,position:video.currentTime}); });
video.addEventListener('timeupdate', () => {
  $('#current-time').textContent = formatTime(video.currentTime);
  $('#seek').value = video.duration ? video.currentTime / video.duration * 100 : 0;
  const now = performance.now();
  if (!videoLoading && videoWantsPlaying && nativeAccompanimentReady && now-lastVideoSyncAt > 750) {
    lastVideoSyncAt = now;
    nativeEvent('syncVideoPlayback',{generation:videoGeneration,position:video.currentTime});
  }
});
$('#seek').addEventListener('input', event => {
  if(!video.duration) return;
  video.currentTime = event.target.value / 100 * video.duration;
  nativeEvent('seekVideo',{generation:videoGeneration,position:video.currentTime});
});
$('#video-volume').addEventListener('input', event => {
  const value = event.target.value / 100;
  video.volume = value;
  nativeEvent('setVideoVolume',{value});
});
const videoCard = $('.video-card');
const fullscreenButton = $('#fullscreen');
async function toggleFullscreen() {
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

$('#theme').addEventListener('change', event => document.body.dataset.theme = event.target.value);
$('#master-volume').addEventListener('input', event => nativeEvent('setMasterVolume', {value:Number(event.target.value)/100}));
const transposeNames = new Map([[0,'C调'],[1,'降D调'],[2,'D调'],[3,'降E调'],[4,'E调'],[5,'F调'],[6,'升F调'],[7,'G调'],[8,'降A调'],[9,'A调'],[10,'降B调'],[11,'B调']]);
const transposeDialog = $('#transpose-dialog');
function updateTransposeDisplay(targetKey) {
  selectedTargetKey = Number(targetKey) || 0;
  const name = transposeNames.get(selectedTargetKey) || 'C调';
  $('#transpose-key-name').textContent = name;
  $('#transpose-current-name').textContent = name;
  $$('#transpose-grid button').forEach(button => button.classList.toggle('active', Number(button.dataset.key) === selectedTargetKey));
}
function openTransposeDialog() {
  updateTransposeDisplay(selectedTargetKey);
  transposeDialog.hidden = false;
}
$('#transpose-key').addEventListener('click', openTransposeDialog);
$('#transpose-grid').addEventListener('click', event => {
  const button = event.target.closest('[data-key]');
  if (!button) return;
  const targetKey = Number(button.dataset.key);
  updateTransposeDisplay(targetKey);
  nativeEvent('setKeyTranspose', {targetKey});
  transposeDialog.hidden = true;
  toast(`演奏调已设为${transposeNames.get(targetKey)}`);
});
$('#cancel-transpose').addEventListener('click', () => { transposeDialog.hidden = true; });
transposeDialog.addEventListener('click', event => { if (event.target === transposeDialog) transposeDialog.hidden = true; });
updateTransposeDisplay(0);
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
function renderTechniqueMappings() {
  if (!techniquePlan.length && currentInstrument) techniquePlan = buildAdaptiveTechniquePlan(currentInstrument.instrumentKey, latestBackendState);
  techniquePlan.forEach(item => { item.mode = item.mode === 'hardware' ? 'hardware' : 'breath'; });
  const list = $('#technique-list');
  if (!techniquePlan.length) {
    list.innerHTML = '<div class="adapter-empty"><b>请先选择音色方案</b><span>风吟只展示当前乐器真正需要的技巧</span></div>';
    $('#technique-more').hidden = true;
    return;
  }
  list.innerHTML = techniquePlan.map(item => {
    const hardware = item.mode === 'hardware';
    const mapped = sourceDescription(item) !== '未设置';
    const modeLabels = {breath:'气息控制',hardware:'硬件控制'};
    return `<article class="technique-card ${item.featured ? '' : 'secondary'}" data-technique-id="${item.id}">
      <div class="technique-card-head"><div class="technique-card-title"><span class="technique-card-icon">${item.icon}</span><div><h3>${escapeHtml(item.name)}</h3><p>${escapeHtml(item.description)}</p></div></div><span class="technique-card-state">${hardware ? (mapped ? '✓ 已映射' : '待映射') : '✓ 已配置'}</span></div>
      <div class="technique-control"><select aria-label="${escapeHtml(item.name)}控制方式">${Object.entries(modeLabels).map(([value,label]) => `<option value="${value}" ${(hardware ? 'hardware' : 'breath') === value ? 'selected' : ''}>${label}</option>`).join('')}</select><button class="learn-source" ${hardware ? '' : 'hidden'}>${Number(window.fengyinTechniqueLearning) === item.index ? '等待操作…' : '映射控制器'}</button></div>
      <div class="technique-strength" ${hardware ? 'hidden' : ''}><span>效果强度</span><input type="range" min="0" max="100" value="${item.strength}" aria-label="${escapeHtml(item.name)}效果强度"><b>${item.strength < 40 ? '自然' : item.strength < 70 ? '适中' : '明显'}</b></div>
      ${item.id === 'growl' && !hardware ? `<label class="technique-threshold">气息触发灵敏度<select class="growl-sensitivity"><option value="easy" ${techniqueSensitivity === 'easy' ? 'selected' : ''}>容易 · 82%</option><option value="standard" ${techniqueSensitivity === 'standard' ? 'selected' : ''}>标准 · 89%</option><option value="hard" ${techniqueSensitivity === 'hard' ? 'selected' : ''}>较难 · 94%</option></select></label>` : ''}
      <p class="recommendation">${hardware ? escapeHtml(sourceDescription(item)) : '气息控制技巧深浅'}</p>
    </article>`;
  }).join('');
  $('#technique-more').hidden = !techniquePlan.some(item => !item.featured);
}
function openTechniqueDialog() {
  if (!currentPluginLoaded || !currentInstrument) return toast('请先选择并加载一个音色方案');
  if (!techniquePlan.length) techniquePlan = buildAdaptiveTechniquePlan(currentInstrument.instrumentKey, latestBackendState);
  $('#technique-dialog-title').textContent = '全局演奏技巧';
  $('#technique-dialog-subtitle').textContent = `${latestBackendState.deviceName || '当前电吹管'} · 映射一次，切换乐器继续使用`;
  $('#technique-ready').textContent = `已自动配置 ${techniquePlan.filter(item => item.mode !== 'off').length} 项`;
  techniqueDialog.hidden = false;
  techniqueHint.textContent = '气息控制可调效果强度；硬件控制请点击“映射控制器”。';
  renderTechniqueMappings();
}
$('#technique-settings').addEventListener('click', openTechniqueDialog);
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
    const wording = {breath:'已改用气息控制',hardware:'请点击“映射控制器”'};
    techniqueHint.textContent = wording[item.mode];
    nativeEvent('setTechniqueConfiguration', {techniqueId:item.id,mode:item.mode,strength:item.strength/100});
    renderTechniqueMappings();
  }
  if (event.target.matches('input[type="range"]')) {
    item.strength = Number(event.target.value);
    nativeEvent('setTechniqueConfiguration', {techniqueId:item.id,mode:item.mode,strength:item.strength/100});
    const label = event.target.closest('.technique-strength')?.querySelector('b');
    if (label) label.textContent = item.strength < 40 ? '自然' : item.strength < 70 ? '适中' : '明显';
  }
});
$('#technique-list').addEventListener('click', event => {
  const button = event.target.closest('.learn-source');
  if (!button) return;
  const item = techniquePlan.find(entry => entry.id === button.closest('.technique-card').dataset.techniqueId);
  if (!item) return;
  window.fengyinTechniqueLearning = item.index;
  techniqueHint.textContent = `正在识别“${item.name}”：请在 10 秒内操作一次吹嘴、摇杆或按键……`;
  $('#learning-dialog-title').textContent = `正在识别“${item.name}”`;
  $('#learning-dialog-message').textContent = '请操作一次希望用于这项技巧的按键、摇杆或吹嘴。普通音符和气息波动会自动忽略。';
  $('#technique-learning-dialog').hidden = false;
  nativeEvent('beginTechniqueLearn', {techniqueId:item.id,toggle:false});
  renderTechniqueMappings();
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
$('#simulate').addEventListener('click', event => {
  simulating = !simulating;
  event.target.textContent = simulating ? '暂停模拟吹奏' : '继续模拟吹奏';
});
$('#record').addEventListener('click', event => {
  if (!recording && video.src && window.__JUCE__?.backend?.emitEvent && !nativeAccompanimentReady) {
    return toast('伴奏音轨尚未准备好，请稍候再开始录音');
  }
  nativeEvent('toggleRecording');
  recording = !recording;
  event.target.textContent = recording ? '■ 停止录音' : '● 开始录音';
  event.target.style.color = recording ? 'var(--danger)' : '';
  toast(recording ? '已开始模拟录音' : '模拟录音已保存');
});
$('#recording-manager').addEventListener('click', () => nativeEvent('showRecordings'));
$('#low-performance').addEventListener('change', event => {
  document.body.classList.toggle('low-performance', event.target.checked);
  toast(event.target.checked ? '已开启流畅模式' : '已恢复精美动画');
});
$('#smart-audio').addEventListener('change', event => {
  nativeEvent('setSmartOptimisation', {enabled:event.target.checked});
  document.querySelector('.smart-badge')?.classList.toggle('off', !event.target.checked);
  toast(event.target.checked ? '智能音频优化已开启' : '智能音频优化已关闭');
});

function trialTimeText(seconds) {
  const safe = Math.max(0, Number(seconds) || 0);
  const days = Math.floor(safe / 86400);
  const hours = Math.ceil((safe % 86400) / 3600);
  return days > 0 ? `${days}天${hours ? `${hours}小时` : ''}` : `${Math.max(1, hours)}小时`;
}

function renderLicenseState(state = {}) {
  const permanent = !!state.activated;
  const trialActive = !!state.trialActive;
  const trialExpired = !!state.trialExpired;
  const lock = $('#license-lock');
  $('#license-title').classList.toggle('green', permanent || trialActive);
  $('#start-trial').hidden = permanent || trialActive || trialExpired;
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
  $('#license-lock-primary').textContent = trialExpired ? '输入永久激活码' : '开始3天完整试用';
  $('#license-lock-kicker').textContent = trialExpired ? '完整试用已结束' : '欢迎使用风吟';
  $('#license-lock-title').textContent = trialExpired ? '激活后继续演奏' : '开始3天完整试用';
  $('#license-lock-message').textContent = trialExpired
    ? '试用期间保存的设置会继续保留。完成永久激活后，全部功能会立即恢复。'
    : '请准备好电吹管和软音源后再开始。点击后将连续计算72小时，全部功能均可使用。';
  $('#license-title').textContent = trialExpired ? '3天试用已结束' : '可开始3天完整试用';
}

function startFullTrial() {
  if (window.__JUCE__?.backend?.emitEvent) {
    nativeEvent('startTrial');
    toast('正在开始3天完整试用…');
    return;
  }
  const startedAt = Date.now();
  localStorage.setItem('fengyin-prototype-trial-start', String(startedAt));
  renderLicenseState({trialActive:true, trialRemainingSeconds:3*24*60*60});
  toast('3天完整试用已开始');
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
  if (!isSupportedInstrument(availableInstruments[index])) return toast('当前版本尚未支持 Kontakt、三体等其他音源，请关注后续版本升级。');
  nativeEvent('loadPlugin', {index}); toast('正在加载所选音源…');
});
$('#instrument-select').addEventListener('change', () => {
  if (customToneMode !== 'create') return;
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
  const index = Number($('#effect-select').value);
  if (!Number.isInteger(index) || index < 0) return toast('请先选择一个外部效果器');
  nativeEvent('loadEffect', {index}); toast('正在加载效果器…');
});
$('#remove-effect').addEventListener('click', () => { nativeEvent('removeEffect'); toast('已移除外部效果器'); });
$('#open-effect').addEventListener('click', () => { toast('正在打开效果器界面…'); nativeEvent('openEffect'); });

function updateBuiltinEffects() {
  const eq = Number($('#eq-tone').value);
  const reverb = Number($('#reverb-mix').value);
  const limiter = Number($('#limiter-ceiling').value);
  const warmth = Number($('#tone-warmth')?.value || 20);
  $('#eq-label').textContent = eq === 0 ? '自然' : `${eq > 0 ? '明亮度 +' : '温暖度 +'}${Math.abs(eq)}`;
  $('#reverb-label').textContent = `强度 ${reverb}%`;
  performanceReverb.value = String(reverb);
  $('#performance-reverb-value').textContent = `${reverb}%`;
  $('#limiter-label').textContent = `上限 ${limiter}%`;
  nativeEvent('setBuiltinEffects', {eq:eq/100,warmth:warmth/100,reverb:reverb/100,limiter:limiter/100});
}
['#eq-tone','#reverb-mix','#limiter-ceiling'].forEach(id => $(id).addEventListener('input', updateBuiltinEffects));
updateBuiltinEffects();
$('#adapter-reconnect')?.addEventListener('click', () => nativeEvent('showMidiSetup'));
$('#adapter-test')?.addEventListener('click', () => toast(currentPluginLoaded ? '请自然吹一个长音，风吟将检查气息、音符和声音' : '请先选择一个音色方案'));
$('#adapter-expression')?.addEventListener('click', () => nativeEvent('showExpressionSettings'));
$('#adapter-techniques')?.addEventListener('click', openTechniqueDialog);
$('#adapter-advanced-toggle')?.addEventListener('click', event => {
  const body = $('#adapter-advanced-body');
  body.classList.toggle('open');
  event.currentTarget.textContent = body.classList.contains('open') ? '收起高级参数⌃' : '高级参数（普通用户无需设置）⌄';
});
$('#wind-status-pill')?.addEventListener('click', () => showPage('wind'));
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
  $('#audio-auto-optimize').disabled = busy;
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
['#audio-output-select','#audio-rate-select','#audio-buffer-select'].forEach(id => $(id).addEventListener('change', () => applyInlineAudioSettings(false)));
$('#audio-auto-optimize').addEventListener('click', () => {
  setAudioControlsBusy(true);
  showAudioOptimisationProgress();
  if (!window.__JUCE__?.backend?.emitEvent) return;
  nativeEvent('optimiseAudioSettings');
});

function showAudioOptimisationProgress() {
  clearInterval(optimisationTimer);
  $('#audio-optimization-dialog').hidden = false;
  $('#optimization-message').textContent = '正在检测当前音源与缓冲区…';
  $('#optimization-progress-bar').style.width = '0%';
  $('#optimization-percent').textContent = '0%';
}

function renderSmartAdapter(state) {
  const connected = !!state.deviceConnected;
  const loaded = !!state.pluginLoaded;
  const instrumentName = state.instrumentChineseName || currentInstrument?.chineseName || state.pluginName || '';
  const instrumentKey = state.instrumentKey || currentInstrument?.instrumentKey || inferInstrumentKey(state.pluginName || '');
  if (loaded && (!techniquePlan.length || currentInstrument?.instrumentKey !== instrumentKey)) techniquePlan = buildAdaptiveTechniquePlan(instrumentKey, state);
  $('#adapter-kicker').textContent = connected ? (state.deviceRecognized ? '已自动识别' : '已连接 · 通用安全模式') : '等待连接设备';
  $('#adapter-device-name').textContent = connected ? (state.deviceName || state.deviceProfileName || '电吹管已连接') : '尚未连接电吹管';
  $('#adapter-summary').textContent = !connected ? '连接后自动识别设备能力，并与当前乐器完成匹配'
    : loaded ? '气息、音符、弯音与当前乐器已完成匹配' : '电吹管已适配；选择音色后继续生成演奏技巧';
  const ready = connected && loaded;
  $('#adapter-overview').classList.toggle('ready', ready);
  $('#adapter-result-title').textContent = ready ? '适配完成，可以开始吹奏' : connected ? '电吹管已准备，等待选择音色' : loaded ? '音色已准备，等待连接电吹管' : '等待电吹管和音色方案';
  $('#adapter-result-detail').textContent = ready ? `已为${state.deviceName || '当前电吹管'}和${instrumentName}完成自动配置。` : '准备完成后，风吟会自动配置气息、起音、弯音和演奏技巧。';
  $('#adapter-breath-state').textContent = !connected ? '等待设备' : state.automaticBreathDetection ? '正在识别…' : '✓ 已优化';
  $('#adapter-breath-value').textContent = !connected ? '自动识别' : state.automaticBreathDetection ? '请自然吹奏' : '自然响应';
  $('#adapter-breath-detail').textContent = connected ? `已识别气息信号，并自动校准强弱范围` : '兼容常见气息信号';
  $('#adapter-note-state').textContent = connected ? '✓ 已保护' : '默认保护';
  $('#adapter-note-value').textContent = connected ? '力度与连奏正常' : '柔和起音';
  $('#adapter-note-detail').textContent = connected ? '已处理首音尖峰、换音衔接和残留音符' : '抑制首音尖峰、重复音符和残留长音';
  $('#adapter-hardware-state').textContent = connected ? '✓ 已识别' : '等待识别';
  const hardware = [];
  if (state.hasBiteSensor) hardware.push('吹嘴咬合');
  if (state.hasThumbController) hardware.push('拇指控制器');
  if (state.hasAssignableButtons) hardware.push('功能键');
  if (state.hasMotionController) hardware.push('动作感应');
  $('#adapter-pitch-value').textContent = connected ? `${hardware.length ? '已匹配控制器' : '通用安全模式'}` : '自动匹配';
  $('#adapter-hardware-detail').textContent = connected ? (hardware.length ? `可用：${hardware.join('、')}` : '没有专用控制器时将使用气息智能补足') : '只使用当前型号真正具备的控制器';
  $('#adapter-instrument-state').textContent = loaded ? '✓ 已识别' : '未加载';
  $('#adapter-instrument-value').textContent = loaded ? instrumentName : '尚未选择音色';
  $('#adapter-instrument-detail').textContent = loaded ? `已生成 ${techniquePlan.length} 项适用技巧` : '选择音色后生成专属技巧方案';
  $('#adapter-techniques').disabled = !loaded;
  $('#adapter-technique-title').textContent = loaded ? `${instrumentName}演奏技巧` : '当前乐器演奏技巧';
  $('#adapter-technique-context').textContent = loaded ? `已自动配置 ${techniquePlan.length} 项，只显示这件乐器真正需要的技巧` : '不会展示与当前乐器无关的技巧';
  const recommendations = loaded ? techniquePlan.filter(item => item.featured).slice(0,4) : [];
  $('#adapter-recommendations').innerHTML = recommendations.length ? recommendations.map(item =>
    `<div class="adapter-recommendation"><small>${escapeHtml(item.name)}</small><strong>${escapeHtml(item.recommendation)}</strong><em>${escapeHtml(item.description)}</em></div>`
  ).join('') : '<div class="adapter-empty"><b>尚未生成技巧方案</b><span>请先选择并加载一个音色方案</span></div>';
}

window.__JUCE__?.backend?.addEventListener('backendState', state => {
  latestBackendState = state || {};
  const connected = !!state.deviceConnected;
  if (Number.isFinite(Number(state.latency))) $('#latency').textContent = `${Number(state.latency).toFixed(1)} ms`;
  if (!audioSettingsApplying && Number.isFinite(Number(state.latency))) {
    const value = Number(state.latency);
    $('#audio-latency-value').textContent = `${value.toFixed(1)} ms · ${value <= 10 ? '优秀' : value <= 20 ? '良好' : '偏高'}`;
    $('#audio-latency-value').classList.toggle('green', value <= 20);
  }
  if (Number.isFinite(Number(state.audioCpu))) $('#cpu').textContent = `${Math.round(Number(state.audioCpu)*100)}%`;
  hardwareBreath = connected ? Math.max(0,Math.min(100,Number(state.breath || 0)*100)) : null;
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
  const backendLimiter = Math.round(Math.max(.6,Math.min(1,Number(state.limiterCeiling ?? .95)))*100);
  if (document.activeElement !== $('#limiter-ceiling')) {
    $('#limiter-ceiling').value = String(backendLimiter);
    $('#limiter-label').textContent = `上限 ${backendLimiter}%`;
  }
  const smartEnabled = state.smartOptimisation !== false;
  if (document.activeElement !== $('#smart-audio')) $('#smart-audio').checked = smartEnabled;
  document.querySelector('.smart-badge')?.classList.toggle('off', !smartEnabled);
  techniqueMappings = Array.isArray(state.techniqueMappings) ? state.techniqueMappings : [];
  window.fengyinTechniqueLearning = Number(state.techniqueLearning ?? -1);
  if (!techniqueDialog.hidden) renderTechniqueMappings();
  const sideTitle = document.querySelector('.sidebar-status b');
  const sideText = document.querySelector('.sidebar-status small');
  const windStatusPill = $('#wind-status-pill');
  const windStatusText = $('#wind-status-text');
  if (sideTitle) sideTitle.textContent = connected ? (state.deviceName || '电吹管已连接') : '尚未连接电吹管';
  if (sideText) sideText.textContent = !connected ? '当前显示模拟演奏效果'
    : state.noteReceived ? '气息与音符信号正常'
    : Number(state.breath || 0) > 0.02 ? '已收到气息，尚未收到音符' : '已连接，等待吹奏信号';
  if (windStatusText) windStatusText.textContent = connected ? '电吹管已连接' : '未连接电吹管';
  if (windStatusPill) windStatusPill.classList.toggle('disconnected', !connected);
  currentPluginLoaded = !!state.pluginLoaded;
  const sound = $('#sound-name');
  if (sound) sound.textContent = currentPluginLoaded
    ? (state.activePresetCustom && state.activePresetName ? state.activePresetName : (state.instrumentChineseName || state.pluginName))
    : '尚未选择音色';
  $('#source-plugin-name').textContent = currentPluginLoaded ? (state.pluginName || '') : '请先到“音色方案”选择并加载';
  currentInstrument = currentPluginLoaded ? {
    name:state.pluginName,
    chineseName:state.instrumentChineseName || state.pluginName,
    instrumentKey:state.instrumentKey || inferInstrumentKey(state.pluginName)
  } : null;
  if (currentPluginLoaded)
    techniquePlan = mergeBackendTechniquePlan(currentInstrument.instrumentKey, state, techniqueMappings);
  if (currentPluginLoaded && (state.instrumentKey || state.pluginName))
    showInstrumentArtwork(state.instrumentKey || inferInstrumentKey(state.pluginName), state.instrumentChineseName || state.pluginName);
  else if (!currentPluginLoaded) {
    currentInstrument = null;
    techniquePlan = [];
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
  $('#load-effect').textContent = state.effectLoading ? '正在加载…' : (effectLoaded ? `✓ ${state.effectName || '已加载'}` : '加载效果');
  availableInstruments = Array.isArray(state.instruments) ? state.instruments : [];
  availableEffects = Array.isArray(state.effects) ? state.effects : [];
  savedPresets = Array.isArray(state.presets) ? state.presets : [];
  if (!state.presetEditing && editingPresetName) {
    editingPresetName = '';
  }
  const signature = JSON.stringify([availableInstruments,availableEffects]);
  if (signature !== pluginListSignature) {
    pluginListSignature = signature;
    if (availableInstruments.length) {
      const supported = availableInstruments.map((item,index) => ({item,index})).filter(({item}) => isSupportedInstrument(item));
      const swam = supported.filter(({item}) => item.brand === 'swam' || item.isSwam === true);
      const kong = supported.filter(({item}) => item.brand === 'kong');
      const unsupported = availableInstruments.map((item,index) => ({item,index})).filter(({item}) => !isSupportedInstrument(item));
      $('#instrument-select').innerHTML = `<option value="">请选择音源</option>${swam.length ? `<optgroup label="西洋乐器 · SWAM">${swam.map(({item,index}) => `<option value="${index}">${escapeHtml(item.label || item.name)}</option>`).join('')}</optgroup>` : ''}${kong.length ? `<optgroup label="中国民乐 · 空音">${kong.map(({item,index}) => `<option value="${index}">${escapeHtml(item.label || item.name)}</option>`).join('')}</optgroup>` : ''}${unsupported.length ? `<optgroup label="暂未支持">${unsupported.map(({item,index}) => `<option class="unsupported-plugin" value="${index}">${escapeHtml(item.label || item.name)}（暂未支持）</option>`).join('')}</optgroup>` : ''}`;
    } else $('#instrument-select').innerHTML = '<option value="">未找到乐器音源</option>';
    $('#effect-select').innerHTML = availableEffects.length
      ? availableEffects.map((name,index) => `<option value="${index}">${escapeHtml(name)}</option>`).join('')
      : '<option value="">未找到外部效果器（可不选）</option>';
  }
  renderToneStyleSwitcher();
  if (currentPluginLoaded && (state.activeToneVariantId || state.toneStyleId)) {
    const styles = toneVariantsForCurrentInstrument();
    const wantedVariant = state.activeToneVariantId || state.toneStyleId;
    const backendStyleIndex = styles.findIndex(style => style.id === wantedVariant || style.id === state.toneStyleId);
    if (backendStyleIndex >= 0 && backendStyleIndex !== currentToneStyleIndex) {
      currentToneStyleIndex = backendStyleIndex;
      renderToneStyleSwitcher();
    }
  }
  const nextPresetSignature = JSON.stringify([savedPresets, availableInstruments, state.kongInstruments]);
  if (nextPresetSignature !== presetListSignature) {
    presetListSignature = nextPresetSignature;
    renderPresets();
  }
  renderLicenseState(state);
  if (state.machineCode) $('#machine-code').textContent = `本机识别码：${state.machineCode}`;
  if (state.appVersion) {
    const versionNode = document.querySelector('.prototype-note');
    if (versionNode) versionNode.textContent = `风吟 ${state.appVersion} · 本地运行，不会保存或上传个人资料。`;
  }
  recording = !!state.recording;
  $('#record').textContent = recording ? '■ 停止录音' : '● 开始录音';
  $('#record').style.color = recording ? 'var(--danger)' : '';
});
$('#copy-machine-code').addEventListener('click', () => {
  if (!latestBackendState.machineCode) return toast('尚未取得本机识别码');
  nativeEvent('copyMachineCode');
  toast('机器码已复制，请发送给安装人员');
});
window.__JUCE__?.backend?.addEventListener('pluginLoadResult', result => {
  if (result?.success) expertDirty = false;
  if (!pendingPresetNavigation || !['plugin','custom-create'].includes(pendingPresetNavigation.kind)) return;
  const pending = pendingPresetNavigation;
  pendingPresetNavigation = null;
  if (!result.success) return toast(result.message || '音色载入失败');
  if (pending.kind === 'custom-create') {
    $('#open-instrument').disabled=false; $('#custom-effects-step').hidden=false;
    $('#instrument-load-state').textContent=`已加载：${pending.name}`; refreshInlineExpert();
    return toast('音源已加载，可以边吹边调整');
  }
  if (pending.styleId) nativeEvent('setToneStyle', {id:pending.styleId});
  showPage('play');
  toast(`已应用：${pending.name}`);
});
window.__JUCE__?.backend?.addEventListener('presetLoadResult', result => {
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
    showPage('play');
    toast(`已载入：${pending.name}`);
  }
});
window.__JUCE__?.backend?.addEventListener('licenseStateResult', result => {
  renderLicenseState(result);
  if (result.activated) $('#license-code').value = '';
  toast(result.message || (result.featuresUnlocked ? '可以开始使用' : '无法继续'));
});
window.__JUCE__?.backend?.addEventListener('editorResult', message => toast(String(message || '')));
window.__JUCE__?.backend?.addEventListener('videoSelected', result => {
  if (!result?.url) return;
  videoLoading = true;
  videoWantsPlaying = false;
  ++videoPlayRequest;
  videoGeneration = Number(result.generation);
  lastVideoSyncAt = 0;
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
  video.src = result.url;
  video.load();
  $('#video-name').textContent = result.name || '伴奏视频';
  $('#change-video').textContent = '更换视频';
  toast('正在载入视频并准备伴奏音轨…');
});
window.__JUCE__?.backend?.addEventListener('videoAudioState', result => {
  if (Number(result?.generation) !== videoGeneration) return;
  nativeAccompanimentReady = !!result?.ready;
  // 后台音轨准备成功后，网页只负责画面，声音统一由风吟输出并进入录音。
  video.muted = true;
  if (result?.message) toast(result.message);
});
window.__JUCE__?.backend?.addEventListener('audioOptimisationResult', message => toast(String(message || '')));
window.__JUCE__?.backend?.addEventListener('audioSettingsState', state => {
  setAudioControlsBusy(!!state?.autoTuning);
  setAudioOptions($('#audio-driver-select'), state?.types, state?.type, value => {
    const name = String(value);
    if (/Low Latency|低延迟/i.test(name)) return `${name}（推荐·共享）`;
    return `${name}（共享）`;
  });
  setAudioOptions($('#audio-output-select'), state?.outputs, state?.output, value => String(value));
  setAudioOptions($('#audio-rate-select'), state?.sampleRates, Number(state?.sampleRate), value => `${Math.round(Number(value))} Hz${Number(value) === 48000 ? '（推荐）' : ''}`);
  setAudioOptions($('#audio-buffer-select'), state?.bufferSizes, Number(state?.bufferSize), value => `${Number(value)}${Number(value) === 128 ? '（推荐）' : Number(value) === 256 ? '（更稳定）' : ''}`);
  const rates = Array.isArray(state?.sampleRates) ? state.sampleRates : [];
  const buffers = Array.isArray(state?.bufferSizes) ? state.bufferSizes : [];
  const lowLatency = !!state?.lowLatencyMode;
  $('#audio-driver-help').textContent = lowLatency
    ? '已使用 Windows 共享低延迟，不影响其他软件发声'
    : '当前为 Windows 共享兼容模式；可点击“自动优化”降低延迟';
  $('#audio-rate-help').textContent = rates.length <= 1
    ? '当前设备仅上报这一个可用采样率'
    : '伴奏视频和软音源推荐使用 48000 Hz';
  $('#audio-buffer-help').textContent = buffers.length <= 1
    ? (lowLatency ? '当前设备驱动仅上报这一个可用缓冲值' : '普通 Windows Audio 由系统固定缓冲；请点击“自动优化”')
    : '128 延迟低；出现爆音时可改为 256';
  const latency = Number(state?.latency);
  const latencyText = Number.isFinite(latency) ? `${latency.toFixed(1)} ms · ${latency <= 10 ? '优秀' : latency <= 20 ? '良好' : '偏高'}` : '尚未取得';
  $('#audio-latency-value').textContent = latencyText;
  $('#audio-latency-value').classList.toggle('green', !Number.isFinite(latency) || latency <= 20);
  const status = $('#audio-apply-status');
  status.textContent = state?.message || (state?.automatic ? '已由风吟自动优化；也可使用上方高级选项手动调整。' : '已使用高级手动设置，软件不会自动覆盖。');
  status.classList.toggle('audio-error', state?.success === false);
  if (state?.autoTuning) {
    if ($('#audio-optimization-dialog').hidden) showAudioOptimisationProgress();
    const percent = Math.floor(Math.min(.99,Math.max(0,Number(state.tuningProgress)||0))*100);
    $('#optimization-progress-bar').style.width = percent+'%';
    $('#optimization-percent').textContent = percent+'%';
  }
  if (!state?.autoTuning && !$('#audio-optimization-dialog').hidden) {
    clearInterval(optimisationTimer);
    $('#audio-optimization-dialog').hidden = true;
  }
  if (state?.message) toast(state.message);
});
window.__JUCE__?.backend?.addEventListener('techniqueLearnResult', result => {
  window.fengyinTechniqueLearning = -1;
  $('#technique-learning-dialog').hidden = true;
  techniqueHint.textContent = result.message || (result.success ? '识别成功' : '没有识别到控制信号');
  toast(techniqueHint.textContent);
  renderTechniqueMappings();
});
nativeEvent('webReady');
renderSmartAdapter({});
renderTechniqueMappings();
clearInstrumentArtwork();
$('.prototype-note').textContent = '风吟 0.13.1 · 本地运行，不会上传个人资料。';
// 浏览器原型使用 localStorage 模拟试用；正式软件等待本地授权服务回传状态。
// 这样已永久激活的用户启动时不会短暂看到“开始试用”遮罩。
if (!window.__JUCE__?.backend?.emitEvent) {
  if(new URLSearchParams(location.search).has('review')) renderLicenseState({activated:true});
  else if(localStorage.getItem('fengyin-prototype-license') === 'active') renderLicenseState({activated:true});
  else {
    const trialStartedAt = Number(localStorage.getItem('fengyin-prototype-trial-start') || 0);
    const trialRemaining = Math.max(0, 3*24*60*60 - Math.floor((Date.now() - trialStartedAt) / 1000));
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

function draw(timestamp = performance.now()) {
  const targetInterval = document.body.classList.contains('video-playing') ? 1000/24 : 1000/60;
  if (timestamp - lastDrawAt < targetInterval) {
    animationFrame = requestAnimationFrame(draw);
    return;
  }
  lastDrawAt = timestamp;
  if (!appFocused && ++unfocusedFrame % 4 !== 0) {
    animationFrame = requestAnimationFrame(draw);
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
  const breath = hardwareBreath ?? (simulating ? 42 + Math.sin(tick*1.7)*20 + Math.sin(tick*.41)*13 : 3);
  const safeBreath = Math.max(2,Math.min(96,breath));
  const styles = getComputedStyle(document.body);
  const a = styles.getPropertyValue('--accent').trim();
  const b = styles.getPropertyValue('--accent-2').trim();
  drawThemeAtmosphere(ratio,a,b);
  drawBreathWave(ratio,safeBreath,a,b);
  const bars = 66;
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
  $('#breath-value').textContent = currentPluginLoaded ? `${Math.round(safeBreath)}%` : '—';
  $('#breath-bar').style.width = currentPluginLoaded ? `${safeBreath}%` : '0%';
  $('#meter-l').style.width = `${Math.min(97,safeBreath+12)}%`;
  $('#meter-r').style.width = `${Math.min(95,safeBreath+7+Math.sin(tick)*6)}%`;
  if (!window.__JUCE__?.backend?.emitEvent) $('#cpu').textContent = `${Math.round(14+Math.abs(Math.sin(tick*.3))*9)}%`;
  animationFrame = requestAnimationFrame(draw);
}
draw();

window.addEventListener('focus', () => { appFocused = true; unfocusedFrame = 0; });
window.addEventListener('blur', () => { appFocused = false; });

window.addEventListener('beforeunload', () => {
  cancelAnimationFrame(animationFrame);
  if(videoUrl) URL.revokeObjectURL(videoUrl);
});
