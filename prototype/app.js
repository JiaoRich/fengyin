const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => [...document.querySelectorAll(selector)];

const titles = {play:'开始演奏',sounds:'音色方案',chain:'音源与音效',wind:'电吹管设置',audio:'声音设置',settings:'软件设置'};
const presets = [
  ['温暖萨克斯','SWAM 高音萨克斯','适合流行与抒情曲目'],
  ['明亮小号','SWAM 小号','清晰、有穿透力'],
  ['清澈长笛','SWAM 长笛','柔和、通透'],
  ['深情单簧管','SWAM 单簧管','温暖而细腻'],
  ['电影感小提琴','SWAM 小提琴','宽广的大厅混响'],
  ['我的新音色','点击开始配置','自定义音源和效果']
];

let simulating = true;
let recording = false;
let animationFrame;
let videoUrl;

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

$$('.nav-item').forEach(button => button.addEventListener('click', () => {
  $$('.nav-item,.page').forEach(el => el.classList.remove('active'));
  button.classList.add('active');
  $(`#page-${button.dataset.page}`).classList.add('active');
  $('#page-title').textContent = titles[button.dataset.page];
}));

$('#preset-grid').innerHTML = presets.map(([name,plugin,desc],index) => `<button class="preset" data-index="${index}"><h3>${name}</h3><b>${plugin}</b><span>${desc}</span></button>`).join('');
$$('.preset').forEach((button,index) => button.addEventListener('click', () => {
  if(index === presets.length-1) return toast('正式版将在这里创建新音色');
  $('#sound-name').textContent = presets[index][1];
  toast(`已应用：${presets[index][0]}`);
}));

$$('.sound-chip').forEach(button => button.addEventListener('click', () => {
  $$('.sound-chip').forEach(el => el.classList.remove('active'));
  button.classList.add('active');
  $('#sound-name').textContent = button.dataset.sound;
}));

const video = $('#video');
$('#video-file').addEventListener('change', event => {
  const file = event.target.files[0];
  if (!file) return;
  if (videoUrl) URL.revokeObjectURL(videoUrl);
  videoUrl = URL.createObjectURL(file);
  video.src = videoUrl;
  video.style.display = 'block';
  $('#video-empty').style.display = 'none';
  $('#video-name').textContent = file.name;
  toast('视频已载入');
});
video.addEventListener('error', () => {
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
video.addEventListener('loadedmetadata', () => $('#duration').textContent = formatTime(video.duration));
video.addEventListener('timeupdate', () => {
  $('#current-time').textContent = formatTime(video.currentTime);
  $('#seek').value = video.duration ? video.currentTime / video.duration * 100 : 0;
});
$('#seek').addEventListener('input', event => { if(video.duration) video.currentTime = event.target.value / 100 * video.duration; });
$('#video-volume').addEventListener('input', event => video.volume = event.target.value / 100);
$('#fullscreen').addEventListener('click', () => $('.video-card').requestFullscreen?.());

function formatTime(seconds) {
  if(!Number.isFinite(seconds)) return '00:00';
  const minutes = Math.floor(seconds / 60).toString().padStart(2,'0');
  const rest = Math.floor(seconds % 60).toString().padStart(2,'0');
  return `${minutes}:${rest}`;
}

$('#theme').addEventListener('change', event => document.body.dataset.theme = event.target.value);
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
    nativeEvent('showActivation');
    return;
  }
  const code = $('#license-code').value.trim();
  if (code.length < 8) return toast('请输入至少 8 位激活码');
  localStorage.setItem('fengyin-prototype-license','active');
  $('#license-title').textContent = '已永久激活';
  $('#license-title').classList.add('green');
  toast('原型激活成功');
});

$('#scan-swam').addEventListener('click', () => { nativeEvent('scanPlugins'); toast('正在扫描 SWAM 与 VST3 音源…'); });
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
  if (state.activated) {
    $('#license-title').textContent = '已永久激活';
    $('#license-title').classList.add('green');
  }
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
