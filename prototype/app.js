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
      nativeEvent('loadPreset', {index});
      return toast(`正在载入：${savedPresets[index]?.name || '我的音色'}`);
    }
    const wanted = presets[index][1].replace('SWAM ', '');
    const found = availableInstruments.findIndex(item => item.label?.includes(wanted));
    if (found >= 0) nativeEvent('loadPlugin', {index: found});
    else if (window.__JUCE__?.backend?.emitEvent) {
      showPage('chain');
      nativeEvent('scanPlugins');
      return toast(`正在查找${wanted}，扫描完成后请选择加载`);
    }
    $('#sound-name').textContent = presets[index][1];
    toast(`已应用：${presets[index][0]}`);
  }));
}
renderPresets();

$$('.sound-chip').forEach(button => button.addEventListener('click', () => {
  $$('.sound-chip').forEach(el => el.classList.remove('active'));
  button.classList.add('active');
  $('#sound-name').textContent = button.dataset.sound;
  const wanted = button.dataset.sound.replace('SWAM ', '');
  const found = availableInstruments.findIndex(item => item.label?.includes(wanted));
  if (found >= 0) nativeEvent('loadPlugin', {index: found});
  else if (window.__JUCE__?.backend?.emitEvent) toast(`尚未找到${wanted}，请先到“音源与音效”扫描`);
}));

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
video.addEventListener('play', () => { $('#play-button').textContent='Ⅱ'; $('#main-play').textContent='Ⅱ 暂停视频'; });
video.addEventListener('pause', () => { $('#play-button').textContent='▶'; $('#main-play').textContent='▶ 播放视频'; });
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
$('#open-instrument').addEventListener('click', () => nativeEvent('openPlugin'));
$('#load-effect').addEventListener('click', () => {
  const index = Number($('#effect-select').value);
  if (!Number.isInteger(index) || index < 0) return toast('请先选择一个外部效果器');
  nativeEvent('loadEffect', {index}); toast('正在加载效果器…');
});
$('#remove-effect').addEventListener('click', () => { nativeEvent('removeEffect'); toast('已移除外部效果器'); });
$('#open-effect').addEventListener('click', () => nativeEvent('openEffect'));
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
  const sideTitle = document.querySelector('.sidebar-status b');
  const sideText = document.querySelector('.sidebar-status small');
  if (sideTitle) sideTitle.textContent = connected ? (state.deviceName || '电吹管已连接') : '尚未连接电吹管';
  if (sideText) sideText.textContent = connected ? '气息与音高信号正常' : '当前显示模拟演奏效果';
  const sound = $('#sound-name');
  if (sound && state.pluginName) sound.textContent = state.pluginName;
  const progress = Math.round((Number(state.scanProgress) || 0) * 100);
  $('#scan-title').textContent = state.scanning ? `正在扫描 ${progress}%` : '重新扫描音源';
  $('#scan-label').textContent = state.scanning ? '请稍候，找到后自动分类' : '点击查找 SWAM / VST3';
  $('#scan-swam').disabled = !!state.scanning;
  if (state.pluginStatus) $('#plugin-status').textContent = state.pluginStatus;
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
window.__JUCE__?.backend?.addEventListener('activationResult', result => {
  if (result.activated) {
    $('#license-title').textContent = '已永久激活';
    $('#license-title').classList.add('green');
    $('#license-code').value = '';
  }
  toast(result.message || (result.activated ? '激活成功' : '无法激活'));
});
nativeEvent('webReady');
if(localStorage.getItem('fengyin-prototype-license') === 'active') {
  $('#license-title').textContent = '已永久激活';
  $('#license-title').classList.add('green');
}

const canvas = $('#spectrum');
const ctx = canvas.getContext('2d');
const notes = [['1','C4'],['2','D4'],['3','E4'],['5','G4'],['6','A4'],['1̇','C5']];
let tick = 0;

function draw() {
  const ratio = window.devicePixelRatio || 1;
  const bounds = canvas.getBoundingClientRect();
  if(canvas.width !== Math.round(bounds.width*ratio) || canvas.height !== Math.round(bounds.height*ratio)) {
    canvas.width = Math.round(bounds.width*ratio); canvas.height = Math.round(bounds.height*ratio);
  }
  ctx.setTransform(ratio,0,0,ratio,0,0);
  ctx.clearRect(0,0,bounds.width,bounds.height);
  tick += simulating ? .045 : .008;
  const breath = simulating ? 42 + Math.sin(tick*1.7)*20 + Math.sin(tick*.41)*13 : 3;
  const safeBreath = Math.max(2,Math.min(96,breath));
  const styles = getComputedStyle(document.body);
  const a = styles.getPropertyValue('--accent').trim();
  const b = styles.getPropertyValue('--accent-2').trim();
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
  const note = notes[Math.floor(tick*.75)%notes.length];
  $('#note').textContent = note[0]; $('#note-name').textContent = note[1];
  animationFrame = requestAnimationFrame(draw);
}
draw();

window.addEventListener('beforeunload', () => {
  cancelAnimationFrame(animationFrame);
  if(videoUrl) URL.revokeObjectURL(videoUrl);
});
