const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => [...document.querySelectorAll(selector)];

const titles = {play:'开始演奏',sounds:'音色方案',chain:'音源与音效',wind:'智能适配',audio:'声音设置',settings:'软件设置'};
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
let favoriteInstruments = [];
let pendingPresetNavigation = null;
let editingPresetName = '';
let techniqueMappings = [];
let latestBackendState = {};
let appFocused = document.hasFocus();
let unfocusedFrame = 0;
let keyCalibrationWasPending = false;
let lastDrawAt = 0;

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
  if (chineseName && !editingPresetName) $('#save-custom').textContent = `保存为“${chineseName}”`;
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
  const create = '<button class="preset preset-create" data-kind="create"><i aria-hidden="true">＋</i><h3>创建我的方案</h3><b>从当前设置开始</b><span>选择音源和效果后保存</span></button>';
  const starters = presets.map(([name,plugin,desc],index) => `<button class="preset" data-kind="starter" data-index="${index}"><h3>${name}</h3><b>${plugin}</b><span>${desc}</span></button>`).join('');
  const scanned = availableInstruments.map((instrument,index) => `<button class="preset preset-scanned" data-kind="scanned" data-index="${index}"><h3>${escapeHtml(instrument.chineseName || instrument.name)}</h3><b>${escapeHtml(instrument.name)}</b><span>扫描生成 · 点击即可载入</span></button>`).join('');
  const saved = savedPresets.map((preset,index) => `<div class="preset-shell"><button class="preset preset-saved" data-kind="saved" data-index="${index}"><h3>${preset.favorite ? '★ ' : ''}${escapeHtml(preset.name)}</h3><b>我的音色方案</b><span>点击恢复已保存的音源与设置</span></button><div class="preset-tools"><button class="preset-edit" type="button" data-index="${index}" aria-label="编辑方案：${escapeHtml(preset.name)}" title="编辑这个方案">编辑</button><button class="preset-delete" type="button" data-index="${index}" aria-label="删除方案：${escapeHtml(preset.name)}" title="删除这个方案">删除</button></div></div>`).join('');
  $('#preset-grid').innerHTML = create + (availableInstruments.length ? scanned : starters) + saved;
  $$('#preset-grid .preset').forEach(button => button.addEventListener('click', () => {
    if (button.dataset.kind === 'create') {
      editingPresetName = '';
      $('#save-custom').textContent = currentInstrument ? `保存为“${currentInstrument.chineseName}”` : '保存为“我的音色”';
      nativeEvent('cancelPresetEdit');
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
    if (button.dataset.kind === 'scanned') {
      const instrument = availableInstruments[index];
      editingPresetName = '';
      nativeEvent('cancelPresetEdit');
      pendingPresetNavigation = {kind:'plugin', name:instrument?.chineseName || instrument?.name || '扫描音源'};
      nativeEvent('loadPlugin', {index});
      return toast(`正在载入：${instrument?.chineseName || instrument?.name || '扫描音源'}`);
    }
    const wanted = presets[index][1].replace('SWAM ', '');
    const found = availableInstruments.findIndex(item => item.label?.includes(wanted));
    if (found >= 0) {
      editingPresetName = '';
      nativeEvent('cancelPresetEdit');
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
  $$('#preset-grid .preset-edit').forEach(button => button.addEventListener('click', event => {
    event.stopPropagation();
    const index = Number(button.dataset.index);
    const name = savedPresets[index]?.name || '我的音色';
    pendingPresetNavigation = {kind:'edit', name};
    nativeEvent('editPreset', {index});
    if (!window.__JUCE__?.backend?.emitEvent) {
      editingPresetName = name;
      $('#save-custom').textContent = `保存对“${name}”的修改`;
      showPage('chain');
    }
    toast(`正在打开：${name}`);
  }));
  $$('#preset-grid .preset-delete').forEach(button => button.addEventListener('click', event => {
    event.stopPropagation();
    const index = Number(button.dataset.index);
    const name = savedPresets[index]?.name || '这个音色方案';
    nativeEvent('deletePreset', {index});
    if (window.__JUCE__?.backend?.emitEvent) toast(`请确认是否删除“${name}”`);
    else {
      savedPresets.splice(index, 1);
      renderPresets();
      toast(`已删除：${name}`);
    }
  }));
}
renderPresets();

const video = $('#video');
const videoFileInput = $('#video-file');
function chooseVideoFile() {
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
video.addEventListener('play', () => { document.body.classList.add('video-playing'); nativeEvent('setVideoPlaybackState',{playing:true,position:video.currentTime}); $('#play-button').textContent='Ⅱ'; $('#main-play').textContent='Ⅱ 暂停视频'; });
video.addEventListener('pause', () => { document.body.classList.remove('video-playing'); nativeEvent('setVideoPlaybackState',{playing:false,position:video.currentTime}); $('#play-button').textContent='▶'; $('#main-play').textContent='▶ 播放视频'; });
video.addEventListener('ended', () => { document.body.classList.remove('video-playing'); nativeEvent('setVideoPlaybackState',{playing:false,position:video.currentTime}); });
video.addEventListener('timeupdate', () => {
  $('#current-time').textContent = formatTime(video.currentTime);
  $('#seek').value = video.duration ? video.currentTime / video.duration * 100 : 0;
  const now = performance.now();
  if (nativeAccompanimentReady && now-lastVideoSyncAt > 750) {
    lastVideoSyncAt = now;
    nativeEvent('syncVideoPlayback',{position:video.currentTime});
  }
});
$('#seek').addEventListener('input', event => {
  if(!video.duration) return;
  video.currentTime = event.target.value / 100 * video.duration;
  nativeEvent('seekVideo',{position:video.currentTime});
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
const transposeNames = new Map([...$('#transpose-key').options].map(option => [Number(option.value), option.textContent]));
const keyCalibrationDialog = $('#key-calibration-dialog');
const keyCalibrationInstruction = $('#key-calibration-instruction');
const startKeyCalibration = $('#start-key-calibration');
function openKeyCalibrationDialog() {
  if (!latestBackendState.deviceConnected) {
    toast('请先连接电吹管，再识别本调');
    return;
  }
  const waiting = !!latestBackendState.keyCalibrationPending;
  keyCalibrationInstruction.textContent = waiting
    ? '正在识别：请按平时演奏 C（Do）的指法，吹一个音。'
    : '请按平时演奏 C（Do）的指法，吹一个音。';
  startKeyCalibration.textContent = waiting ? '等待吹奏…' : '开始识别';
  startKeyCalibration.disabled = waiting;
  keyCalibrationDialog.hidden = false;
}
$('#transpose-key').addEventListener('change', event => {
  const targetKey = Number(event.target.value);
  nativeEvent('setKeyTranspose', {targetKey});
  if (!latestBackendState.deviceConnected) {
    toast('已选择目标演奏调；连接电吹管后请先识别本调');
  } else if (!latestBackendState.keyCalibrated) {
    openKeyCalibrationDialog();
  } else {
    toast(`目标演奏调已设为${transposeNames.get(targetKey) || '所选调性'}`);
  }
});
$('#key-calibration').addEventListener('click', openKeyCalibrationDialog);
startKeyCalibration.addEventListener('click', () => {
  nativeEvent('beginKeyCalibration');
  keyCalibrationInstruction.textContent = '正在识别：请按平时演奏 C（Do）的指法，吹一个音。';
  startKeyCalibration.textContent = '等待吹奏…';
  startKeyCalibration.disabled = true;
});
function closeKeyCalibrationDialog() {
  if (latestBackendState.keyCalibrationPending) nativeEvent('cancelKeyCalibration');
  keyCalibrationDialog.hidden = true;
}
$('#cancel-key-calibration').addEventListener('click', closeKeyCalibrationDialog);
keyCalibrationDialog.addEventListener('click', event => {
  if (event.target === keyCalibrationDialog) closeKeyCalibrationDialog();
});
const performanceReverb = $('#performance-reverb');
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
  const hasCapabilityState = techniqueMappings.length > 0;
  $$('.technique-row').forEach(row => {
    const technique = Number(row.dataset.technique);
    const mapping = techniqueMappings.find(item => Number(item.technique) === technique);
    const mapped = !!mapping && Number(mapping.sourceType) !== 0;
    const relevant = mapping?.relevant !== false;
    const pluginSupported = mapping?.pluginSupported !== false;
    row.hidden = !hasCapabilityState || !relevant;
    row.classList.toggle('unavailable', !pluginSupported);
    row.querySelector('.mapping-status').textContent = !pluginSupported ? '当前音源不支持'
      : mapped ? sourceDescription(mapping)
      : mapping?.hardwareAvailable ? `推荐：${mapping.recommendedSource}` : '可手动识别';
    row.querySelector('select').value = mapping?.toggle ? 'toggle' : 'hold';
    row.querySelector('select').disabled = !pluginSupported;
    row.querySelector('.remove-technique').hidden = !mapped;
    row.classList.toggle('unsupported', !pluginSupported);
    row.querySelector('.learn-technique').disabled = !pluginSupported;
    row.querySelector('.learn-technique').textContent = Number(mapping?.technique) === Number(row.dataset.technique)
      && Number(window.fengyinTechniqueLearning) === technique ? '等待操作…' : (mapped ? '重新识别' : '开始识别');
  });
}
function openTechniqueDialog() {
  techniqueDialog.hidden = false;
  techniqueHint.textContent = '这里只显示当前乐器实际需要的技巧，并按已连接电吹管的硬件给出推荐。';
  renderTechniqueMappings();
}
$('#technique-settings').addEventListener('click', openTechniqueDialog);
$('#close-technique-dialog').addEventListener('click', () => {
  nativeEvent('cancelTechniqueLearn');
  techniqueDialog.hidden = true;
});
techniqueDialog.addEventListener('click', event => {
  if (event.target === techniqueDialog) {
    nativeEvent('cancelTechniqueLearn');
    techniqueDialog.hidden = true;
  }
});
$$('.learn-technique').forEach(button => button.addEventListener('click', () => {
  const row = button.closest('.technique-row');
  const technique = Number(row.dataset.technique);
  window.fengyinTechniqueLearning = technique;
  techniqueHint.textContent = '正在识别：请在 10 秒内操作希望使用的电吹管按键、吹嘴或摇杆……';
  nativeEvent('beginTechniqueLearn', {technique, toggle:row.querySelector('select').value === 'toggle'});
  renderTechniqueMappings();
}));
$$('.remove-technique').forEach(button => button.addEventListener('click', () => {
  const technique = Number(button.closest('.technique-row').dataset.technique);
  nativeEvent('removeTechniqueMapping', {technique});
  techniqueMappings = techniqueMappings.filter(item => Number(item.technique) !== technique);
  renderTechniqueMappings();
  toast('已取消这项技巧映射');
}));
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
  performanceReverb.value = String(reverb);
  $('#performance-reverb-value').textContent = `${reverb}%`;
  $('#limiter-label').textContent = `上限 ${limiter}%`;
  nativeEvent('setBuiltinEffects', {eq:eq/100,reverb:reverb/100,limiter:limiter/100});
}
['#eq-tone','#reverb-mix','#limiter-ceiling'].forEach(id => $(id).addEventListener('input', updateBuiltinEffects));
updateBuiltinEffects();
$('#adapter-reconnect')?.addEventListener('click', () => nativeEvent('showMidiSetup'));
$('#adapter-expression')?.addEventListener('click', () => nativeEvent('showExpressionSettings'));
$('#adapter-techniques')?.addEventListener('click', openTechniqueDialog);
$('#adapter-advanced-toggle')?.addEventListener('click', event => {
  const body = $('#adapter-advanced-body');
  body.classList.toggle('open');
  event.currentTarget.textContent = body.classList.contains('open') ? '收起高级参数⌃' : '高级参数（普通用户无需设置）⌄';
});
$('#wind-status-pill')?.addEventListener('click', () => showPage('wind'));
document.querySelectorAll('#page-audio button').forEach(button => button.addEventListener('click', () => nativeEvent('showAudioSettings')));

const techniqueNames = ['嘶吼音','颤音','花舌'];
function renderSmartAdapter(state) {
  const connected = !!state.deviceConnected;
  const loaded = !!state.pluginLoaded;
  $('#adapter-kicker').textContent = connected ? (state.deviceRecognized ? '已自动识别' : '已连接 · 通用安全模式') : '等待连接设备';
  $('#adapter-device-name').textContent = connected ? (state.deviceName || state.deviceProfileName || '电吹管已连接') : '尚未连接电吹管';
  $('#adapter-summary').textContent = !connected ? '连接后将自动识别气息、弯音和可用硬件'
    : state.keyCalibrationPending ? '请按平时演奏 C（Do）的指法，吹一个音'
    : !state.keyCalibrated ? '气息与控制器已适配；使用移调前需要识别吹管本调'
    : loaded ? '气息、音符、弯音与当前 SWAM 乐器已完成匹配' : '电吹管已适配；加载 SWAM 后将继续匹配演奏技巧';
  $('#adapter-breath-state').textContent = !connected ? '等待设备' : state.automaticBreathDetection ? '正在识别…' : '✓ 已优化';
  $('#adapter-breath-value').textContent = !connected ? '自动识别' : state.automaticBreathDetection ? '请自然吹奏' : '自然响应';
  $('#adapter-breath-detail').textContent = connected ? `已使用 CC${Number(state.breathController ?? 2)}，并启用首音柔化保护` : '兼容 CC2、CC11 等常见气息信号';
  $('#adapter-hardware-state').textContent = connected ? '✓ 已识别' : '等待识别';
  const hardware = [];
  if (state.hasBiteSensor) hardware.push('吹嘴咬合');
  if (state.hasThumbController) hardware.push('拇指控制器');
  if (state.hasAssignableButtons) hardware.push('功能键');
  if (state.hasMotionController) hardware.push('动作感应');
  $('#adapter-hardware-detail').textContent = connected ? (hardware.length ? `可用于技巧：${hardware.join('、')}` : '未预设控制器，可通过操作一次完成识别') : '只推荐当前型号实际具备的硬件';
  $('#adapter-techniques').disabled = !loaded;
  const recommendations = techniqueMappings.filter(item => item.relevant !== false && item.pluginSupported !== false);
  $('#adapter-recommendations').innerHTML = recommendations.length ? recommendations.map(item => {
    const mapped = Number(item.sourceType) !== 0;
    const recommendation = mapped ? `已映射：${sourceDescription(item)}`
      : item.hardwareAvailable ? `推荐：${escapeHtml(item.recommendedSource || '可用控制器')}` : '可点击“编辑映射”自动识别';
    return `<div class="adapter-recommendation"><small>${escapeHtml(techniqueNames[Number(item.technique)] || '演奏技巧')}</small><strong>${recommendation}</strong><em>${escapeHtml(item.recommendationReason || '按当前设备与乐器智能匹配')}</em></div>`;
  }).join('') : `<div class="adapter-empty">${loaded ? '当前音源没有开放可映射的技巧参数' : '尚未加载可识别的 SWAM 乐器'}</div>`;
}

window.__JUCE__?.backend?.addEventListener('backendState', state => {
  latestBackendState = state || {};
  const connected = !!state.deviceConnected;
  if (Number.isFinite(Number(state.latency))) $('#latency').textContent = `${Number(state.latency).toFixed(1)} ms`;
  if (Number.isFinite(Number(state.audioCpu))) $('#cpu').textContent = `${Math.round(Number(state.audioCpu)*100)}%`;
  hardwareBreath = connected ? Math.max(0,Math.min(100,Number(state.breath || 0)*100)) : null;
  const targetKey = Number(state.targetKey || 0);
  if ($('#transpose-key').value !== String(targetKey)) $('#transpose-key').value = String(targetKey);
  const calibrationPending = connected && !!state.keyCalibrationPending;
  const keyCalibrated = connected && !!state.keyCalibrated;
  const calibrationButton = $('#key-calibration');
  calibrationButton.classList.toggle('pending', calibrationPending);
  calibrationButton.classList.toggle('ready', keyCalibrated && !calibrationPending);
  calibrationButton.textContent = !connected || !keyCalibrated
    ? (calibrationPending ? '请吹 C（Do）音' : '识别本调')
    : `本调：${transposeNames.get(Number(state.sourceKey || 0)) || '已识别'}`;
  if (calibrationPending && !keyCalibrationWasPending) toast('请按 C（Do）指法吹一个音');
  if (!calibrationPending && keyCalibrationWasPending && keyCalibrated) {
    keyCalibrationDialog.hidden = true;
    startKeyCalibration.disabled = false;
    startKeyCalibration.textContent = '开始识别';
    toast(`本调识别完成，已自动换算到${transposeNames.get(targetKey) || '目标调'}`);
  }
  keyCalibrationWasPending = calibrationPending;
  const backendReverb = Math.round(Math.max(0,Math.min(.6,Number(state.reverbMix ?? .28)))*100);
  if (document.activeElement !== performanceReverb && document.activeElement !== $('#reverb-mix')) {
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
  renderSmartAdapter(state);
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
  if (!state.presetEditing && editingPresetName) {
    editingPresetName = '';
    $('#save-custom').textContent = currentInstrument ? `保存为“${currentInstrument.chineseName}”` : '保存为“我的音色”';
  }
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
  const nextPresetSignature = JSON.stringify([savedPresets, availableInstruments]);
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
$('#copy-machine-code').addEventListener('click', () => {
  if (!latestBackendState.machineCode) return toast('尚未取得本机识别码');
  nativeEvent('copyMachineCode');
  toast('机器码已复制，请发送给安装人员');
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
  if (!pendingPresetNavigation || !['preset','edit'].includes(pendingPresetNavigation.kind)) return;
  const pending = pendingPresetNavigation;
  pendingPresetNavigation = null;
  if (!result.success) return toast(result.message || '音色方案载入失败');
  if (pending.kind === 'edit') {
    editingPresetName = pending.name;
    $('#save-custom').textContent = `保存对“${pending.name}”的修改`;
    showPage('chain');
    toast(`可以修改“${pending.name}”，完成后点击保存修改`);
  } else {
    showPage('play');
    toast(`已载入：${pending.name}`);
  }
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
window.__JUCE__?.backend?.addEventListener('videoSelected', result => {
  if (!result?.url) return;
  selectedVideoFile = null;
  triedVideoDataFallback = true;
  nativeAccompanimentReady = false;
  video.pause();
  video.muted = false;
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
  nativeAccompanimentReady = !!result?.ready;
  // 后台音轨准备成功后，网页只负责画面，声音统一由风吟输出并进入录音。
  video.muted = nativeAccompanimentReady;
  if (result?.message) toast(result.message);
});
window.__JUCE__?.backend?.addEventListener('audioOptimisationResult', message => toast(String(message || '')));
window.__JUCE__?.backend?.addEventListener('techniqueLearnResult', result => {
  window.fengyinTechniqueLearning = -1;
  techniqueHint.textContent = result.message || (result.success ? '识别成功' : '没有识别到控制信号');
  toast(techniqueHint.textContent);
  renderTechniqueMappings();
});
nativeEvent('webReady');
renderSmartAdapter({});
renderTechniqueMappings();
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
  $('#breath-value').textContent = `${Math.round(safeBreath)}%`;
  $('#breath-bar').style.width = `${safeBreath}%`;
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
