const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => [...document.querySelectorAll(selector)];

const titles = {play:'开始演奏',sounds:'音色方案',chain:'音源与音效',wind:'电吹管设置',audio:'声音设置',settings:'软件设置'};
const presets = [
  ['温暖萨克斯','SWAM 高音萨克斯','适合流行与抒情曲目'],
  ['明亮小号','SWAM 小号','清晰、有穿透力'],
  ['清澈长笛','SWAM 长笛','柔和、通透'],
  ['深情单簧管','SWAM 单簧管','温暖而细腻'],
  ['电影感小提琴','SWAM 小提琴','宽广的大厅混响']
];

let simulating = true;
let recording = false;
let animationFrame;
let videoUrl;
let selectedVideoFile;
let triedVideoDataFallback = false;
let availableInstruments = [];
let availableEffects = [];
let pluginListSignature = '';
let savedPresets = [];
let presetListSignature = '';
let hardwareBreath = null;
let currentArtworkKey = '';
let currentInstrument = null;
let currentPluginLoaded = false;
let favoriteInstruments = [];
let pendingPresetNavigation = null;
let appFocused = document.hasFocus();
let unfocusedFrame = 0;

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

function showInstrumentArtwork(key, chineseName = '') {
  const picture = $('#instrument-picture');
  const artwork = instrumentArtwork[key] || instrumentArtwork['alto-sax'];
  if (chineseName) $('#save-custom').textContent = `保存为“${chineseName}”`;
  picture.alt = `当前乐器：${chineseName || 'SWAM 乐器'}`;
  if (currentArtworkKey === key) return;
  currentArtworkKey = key;
  picture.classList.add('changing');
  picture.src = `../assets/instruments/${artwork}`;
  window.setTimeout(() => picture.classList.remove('changing'), 130);
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
}

$$('.nav-item').forEach(button => button.addEventListener('click', () => showPage(button.dataset.page)));

function escapeHtml(value) {
  return String(value ?? '').replace(/[&<>"']/g, character => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[character]));
}

function loadFavoriteInstruments() {
  try {
    const saved = JSON.parse(localStorage.getItem('fengyin-favorite-instruments-v1') || '[]');
    favoriteInstruments = Array.isArray(saved) ? saved.filter(item => item?.name).slice(0, 3) : [];
  } catch (_) {
    favoriteInstruments = [];
  }
}

function saveFavoriteInstruments() {
  localStorage.setItem('fengyin-favorite-instruments-v1', JSON.stringify(favoriteInstruments.slice(0, 3)));
}

function renderFavoriteInstruments() {
  const container = $('#favorite-instruments');
  container.classList.toggle('empty', favoriteInstruments.length === 0);
  container.innerHTML = favoriteInstruments.length
    ? favoriteInstruments.map(item => `<button class="sound-chip" data-plugin-name="${escapeHtml(item.name)}">${escapeHtml(item.chineseName || item.name)}</button>`).join('')
    : '<span class="favorite-empty">尚未收藏常用乐器</span>';
  $$('#favorite-instruments .sound-chip').forEach(button => button.addEventListener('click', () => {
    const index = availableInstruments.findIndex(item => item.name === button.dataset.pluginName);
    if (index < 0) return toast('当前扫描结果中找不到这个乐器，请先重新扫描音源');
    nativeEvent('loadPlugin', {index});
    toast(`正在加载：${button.textContent}`);
  }));
  const star = $('#favorite-current');
  const isFavorite = !!currentInstrument && favoriteInstruments.some(item => item.name === currentInstrument.name);
  star.textContent = isFavorite ? '★' : '☆';
  star.classList.toggle('active', isFavorite);
  star.disabled = !currentPluginLoaded || !currentInstrument;
  star.setAttribute('aria-label', isFavorite ? '取消收藏当前乐器' : '收藏当前乐器');
}

loadFavoriteInstruments();
renderFavoriteInstruments();
$('#favorite-current').addEventListener('click', () => {
  if (!currentPluginLoaded || !currentInstrument) return toast('请先加载一个乐器音源');
  const index = favoriteInstruments.findIndex(item => item.name === currentInstrument.name);
  if (index >= 0) {
    favoriteInstruments.splice(index, 1);
    toast(`已取消收藏：${currentInstrument.chineseName}`);
  } else {
    if (favoriteInstruments.length >= 3) return toast('最多收藏三个常用乐器，请先取消一个');
    favoriteInstruments.push({...currentInstrument});
    toast(`已收藏：${currentInstrument.chineseName}`);
  }
  saveFavoriteInstruments();
  renderFavoriteInstruments();
});

function renderPresets() {
  const starters = presets.map(([name,plugin,desc],index) => `<button class="preset" data-kind="starter" data-index="${index}"><h3>${name}</h3><b>${plugin}</b><span>${desc}</span></button>`).join('');
  const saved = savedPresets.map((preset,index) => `<button class="preset" data-kind="saved" data-index="${index}"><h3>${preset.favorite ? '★ ' : ''}${escapeHtml(preset.name)}</h3><b>我的音色方案</b><span>点击恢复已保存的音源与设置</span></button>`).join('');
  $('#preset-grid').innerHTML = starters + saved + '<button class="preset" data-kind="create"><h3>我的新音色</h3><b>点击开始配置</b><span>自定义音源和效果</span></button>';
  $$('#preset-grid .preset').forEach(button => button.addEventListener('click', () => {
    if (button.dataset.kind === 'create') {
      showPage('chain');
      toast('请扫描并选择音源，调好效果后即可开始演奏');
      if (!availableInstruments.length) nativeEvent('scanPlugins');
      return;
    }
    const index = Number(button.dataset.index);
    if (button.dataset.kind === 'saved') {
      pendingPresetNavigation = {kind:'preset', name:savedPresets[index]?.name || '我的音色'};
      nativeEvent('loadPreset', {index});
      return toast(`正在载入：${savedPresets[index]?.name || '我的音色'}`);
    }
    const wanted = presets[index][1].replace('SWAM ', '');
    const found = availableInstruments.findIndex(item => item.label?.includes(wanted));
    if (found >= 0) {
      pendingPresetNavigation = {kind:'plugin', name:presets[index][0]};
      nativeEvent('loadPlugin', {index: found});
    }
    else if (window.__JUCE__?.backend?.emitEvent) {
      showPage('chain');
      nativeEvent('scanPlugins');
      return toast(`正在查找${wanted}，扫描完成后请选择加载`);
    }
    if (!window.__JUCE__?.backend?.emitEvent) {
      $('#sound-name').textContent = wanted;
      showInstrumentArtwork(inferInstrumentKey(presets[index][1]), wanted);
      showPage('play');
      toast(`已应用：${presets[index][0]}`);
    } else if (found >= 0) toast(`正在载入：${presets[index][0]}`);
  }));
}
renderPresets();

const video = $('#video');
$('#video-file').addEventListener('change', event => {
  const file = event.target.files[0];
  if (!file) return;
  selectedVideoFile = file;
  triedVideoDataFallback = false;
  video.pause();
  video.style.display = 'none';
  $('#video-empty').style.display = 'grid';
  if (videoUrl) URL.revokeObjectURL(videoUrl);
  videoUrl = URL.createObjectURL(file);
  video.src = videoUrl;
  video.load();
  $('#video-name').textContent = file.name;
  toast('正在载入视频…');
});
video.addEventListener('loadedmetadata', () => {
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
  video.paused ? video.play() : video.pause();
}
$('#play-button').addEventListener('click', toggleVideo);
$('#main-play').addEventListener('click', toggleVideo);
video.addEventListener('play', () => { document.body.classList.add('video-playing'); $('#play-button').textContent='Ⅱ'; $('#main-play').textContent='Ⅱ 暂停视频'; });
video.addEventListener('pause', () => { document.body.classList.remove('video-playing'); $('#play-button').textContent='▶'; $('#main-play').textContent='▶ 播放视频'; });
video.addEventListener('ended', () => document.body.classList.remove('video-playing'));
video.addEventListener('timeupdate', () => {
  $('#current-time').textContent = formatTime(video.currentTime);
  $('#seek').value = video.duration ? video.currentTime / video.duration * 100 : 0;
});
$('#seek').addEventListener('input', event => { if(video.duration) video.currentTime = event.target.value / 100 * video.duration; });
$('#video-volume').addEventListener('input', event => video.volume = event.target.value / 100);
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
const playPage = $('#page-play');
const stageGrid = $('.stage-grid');
const layoutResizer = $('#layout-resizer');
const lowerStage = $('#lower-stage');
function setStageHeight(clientY) {
  const bounds = playPage.getBoundingClientRect();
  const minimum = window.innerHeight <= 850 ? 280 : 300;
  const dividerHeight = layoutResizer.getBoundingClientRect().height;
  const lowerMinimum = parseFloat(getComputedStyle(lowerStage).minHeight) || 180;
  const maximum = Math.max(minimum, bounds.height - dividerHeight - lowerMinimum);
  const next = Math.max(minimum, Math.min(maximum, clientY - bounds.top));
  stageGrid.style.flexBasis = `${next}px`;
}
layoutResizer.addEventListener('pointerdown', event => {
  layoutResizer.setPointerCapture(event.pointerId);
  setStageHeight(event.clientY);
});
layoutResizer.addEventListener('pointermove', event => {
  if (layoutResizer.hasPointerCapture(event.pointerId)) setStageHeight(event.clientY);
});
layoutResizer.addEventListener('keydown', event => {
  if (!['ArrowUp','ArrowDown'].includes(event.key)) return;
  event.preventDefault();
  const bounds = playPage.getBoundingClientRect();
  const minimum = window.innerHeight <= 850 ? 280 : 300;
  const dividerHeight = layoutResizer.getBoundingClientRect().height;
  const lowerMinimum = parseFloat(getComputedStyle(lowerStage).minHeight) || 180;
  const next = stageGrid.getBoundingClientRect().height + (event.key === 'ArrowDown' ? 16 : -16);
  stageGrid.style.flexBasis = `${Math.max(minimum,Math.min(bounds.height - dividerHeight - lowerMinimum,next))}px`;
});
$('#master-volume').addEventListener('input', event => nativeEvent('setMasterVolume', {value:Number(event.target.value)/100}));
$('#simulate').addEventListener('click', event => {
  simulating = !simulating;
  event.target.textContent = simulating ? '暂停模拟吹奏' : '继续模拟吹奏';
});
$('#record').addEventListener('click', event => {
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
  $('#license-title').textContent = '已永久激活';
  $('#license-title').classList.add('green');
  toast('原型激活成功');
});

$('#scan-swam').addEventListener('click', () => { nativeEvent('scanPlugins'); $('#scan-title').textContent='正在扫描…'; $('#scan-label').textContent='请稍候，扫描不会阻塞演奏'; toast('正在扫描 SWAM 与 VST3 音源…'); });
$('#load-instrument').addEventListener('click', () => {
  const index = Number($('#instrument-select').value);
  if (!Number.isInteger(index) || index < 0) return toast('请先扫描并选择一个乐器音源');
  nativeEvent('loadPlugin', {index}); toast('正在加载所选音源…');
});
$('#open-instrument').addEventListener('click', () => { toast('正在打开音源界面…'); nativeEvent('openPlugin'); });
$('#load-effect').addEventListener('click', () => {
  const index = Number($('#effect-select').value);
  if (!Number.isInteger(index) || index < 0) return toast('请先选择一个外部效果器');
  nativeEvent('loadEffect', {index}); toast('正在加载效果器…');
});
$('#remove-effect').addEventListener('click', () => { nativeEvent('removeEffect'); toast('已移除外部效果器'); });
$('#open-effect').addEventListener('click', () => { toast('正在打开效果器界面…'); nativeEvent('openEffect'); });
$('#save-custom').addEventListener('click', () => { nativeEvent('savePreset'); toast('请输入音色方案名称并保存'); });

function updateBuiltinEffects() {
  const eq = Number($('#eq-tone').value);
  const reverb = Number($('#reverb-mix').value);
  const limiter = Number($('#limiter-ceiling').value);
  $('#eq-label').textContent = eq === 0 ? '自然' : `${eq > 0 ? '明亮度 +' : '温暖度 +'}${Math.abs(eq)}`;
  $('#reverb-label').textContent = `强度 ${reverb}%`;
  $('#limiter-label').textContent = `上限 ${limiter}%`;
  nativeEvent('setBuiltinEffects', {eq:eq/100,reverb:reverb/100,limiter:limiter/100});
}
['#eq-tone','#reverb-mix','#limiter-ceiling'].forEach(id => $(id).addEventListener('input', updateBuiltinEffects));
updateBuiltinEffects();
const windButtons = [...document.querySelectorAll('#page-wind button')];
windButtons[0]?.addEventListener('click', () => nativeEvent('showMidiSetup'));
windButtons[1]?.addEventListener('click', () => nativeEvent('showExpressionSettings'));
document.querySelectorAll('#page-audio button').forEach(button => button.addEventListener('click', () => nativeEvent('showAudioSettings')));

window.__JUCE__?.backend?.addEventListener('backendState', state => {
  const connected = !!state.deviceConnected;
  hardwareBreath = connected ? Math.max(0,Math.min(100,Number(state.breath || 0)*100)) : null;
  const sideTitle = document.querySelector('.sidebar-status b');
  const sideText = document.querySelector('.sidebar-status small');
  const windStatusPill = $('#wind-status-pill');
  const windStatusText = $('#wind-status-text');
  if (sideTitle) sideTitle.textContent = connected ? (state.deviceName || '电吹管已连接') : '尚未连接电吹管';
  if (sideText) sideText.textContent = connected ? '气息与音高信号正常' : '当前显示模拟演奏效果';
  if (windStatusText) windStatusText.textContent = connected ? '电吹管已连接' : '未连接电吹管';
  if (windStatusPill) windStatusPill.classList.toggle('disconnected', !connected);
  currentPluginLoaded = !!state.pluginLoaded;
  const sound = $('#sound-name');
  if (sound) sound.textContent = currentPluginLoaded ? (state.instrumentChineseName || state.pluginName) : '安全测试音源';
  $('#source-plugin-name').textContent = currentPluginLoaded ? (state.pluginName || '') : '尚未加载 SWAM 音源';
  currentInstrument = currentPluginLoaded ? {
    name:state.pluginName,
    chineseName:state.instrumentChineseName || state.pluginName,
    instrumentKey:state.instrumentKey || inferInstrumentKey(state.pluginName)
  } : null;
  if (state.instrumentKey || state.pluginName)
    showInstrumentArtwork(state.instrumentKey || inferInstrumentKey(state.pluginName), state.instrumentChineseName || state.pluginName);
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
  const signature = JSON.stringify([availableInstruments,availableEffects]);
  if (signature !== pluginListSignature) {
    pluginListSignature = signature;
    $('#instrument-select').innerHTML = availableInstruments.length
      ? availableInstruments.map((item,index) => `<option value="${index}">${escapeHtml(item.label || item.name)}</option>`).join('')
      : '<option value="">未找到乐器音源</option>';
    $('#effect-select').innerHTML = availableEffects.length
      ? availableEffects.map((name,index) => `<option value="${index}">${escapeHtml(name)}</option>`).join('')
      : '<option value="">未找到外部效果器（可不选）</option>';
  }
  renderFavoriteInstruments();
  const nextPresetSignature = JSON.stringify(savedPresets);
  if (nextPresetSignature !== presetListSignature) {
    presetListSignature = nextPresetSignature;
    renderPresets();
  }
  if (state.activated) {
    $('#license-title').textContent = '已永久激活';
    $('#license-title').classList.add('green');
  }
  if (state.machineCode) $('#machine-code').textContent = `本机识别码：${state.machineCode}`;
  recording = !!state.recording;
  $('#record').textContent = recording ? '■ 停止录音' : '● 开始录音';
  $('#record').style.color = recording ? 'var(--danger)' : '';
});
window.__JUCE__?.backend?.addEventListener('pluginLoadResult', result => {
  if (!pendingPresetNavigation || pendingPresetNavigation.kind !== 'plugin') return;
  const pending = pendingPresetNavigation;
  pendingPresetNavigation = null;
  if (!result.success) return toast(result.message || '音色载入失败');
  showPage('play');
  toast(`已应用：${pending.name}`);
});
window.__JUCE__?.backend?.addEventListener('presetLoadResult', result => {
  if (!pendingPresetNavigation || pendingPresetNavigation.kind !== 'preset') return;
  const pending = pendingPresetNavigation;
  pendingPresetNavigation = null;
  if (!result.success) return toast(result.message || '音色方案载入失败');
  showPage('play');
  toast(`已载入：${pending.name}`);
});
window.__JUCE__?.backend?.addEventListener('activationResult', result => {
  if (result.activated) {
    $('#license-title').textContent = '已永久激活';
    $('#license-title').classList.add('green');
    $('#license-code').value = '';
  }
  toast(result.message || (result.activated ? '激活成功' : '无法激活'));
});
window.__JUCE__?.backend?.addEventListener('editorResult', message => toast(String(message || '')));
nativeEvent('webReady');
showInstrumentArtwork('soprano-sax', '高音萨克斯');
if(localStorage.getItem('fengyin-prototype-license') === 'active') {
  $('#license-title').textContent = '已永久激活';
  $('#license-title').classList.add('green');
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

function draw() {
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
  $('#breath-value').textContent = `${Math.round(safeBreath)}%`;
  $('#breath-bar').style.width = `${safeBreath}%`;
  $('#meter-l').style.width = `${Math.min(97,safeBreath+12)}%`;
  $('#meter-r').style.width = `${Math.min(95,safeBreath+7+Math.sin(tick)*6)}%`;
  $('#cpu').textContent = `${Math.round(14+Math.abs(Math.sin(tick*.3))*9)}%`;
  animationFrame = requestAnimationFrame(draw);
}
draw();

window.addEventListener('focus', () => { appFocused = true; unfocusedFrame = 0; });
window.addEventListener('blur', () => { appFocused = false; });

window.addEventListener('beforeunload', () => {
  cancelAnimationFrame(animationFrame);
  if(videoUrl) URL.revokeObjectURL(videoUrl);
});
