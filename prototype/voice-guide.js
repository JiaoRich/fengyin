/* Offline, reviewed speech. No browser/cloud TTS in the installed application. */
(() => {
  const updates=[
    ['REC_SAVED','录制已保存，可以在我的录制中查看。'],
    ['REC_AUDIO','准备开始录音。'],
    ['GUIDE_OUTPUT','请确认声音输出是您正在使用的扬声器或耳机。'],
    ['GUIDE_MATCH','点击开始匹配。保持管身不动，不咬嘴，吹一个气息由弱到强的长音。'],
    ['GUIDE_TRY','请吹几个音，试试轻吹和强吹。能听到声音后，就可以开始演奏了。'],
    ['GUIDE_TECH','选择咬嘴控制或按键控制，再点击映射。需要移除时，点击清除映射。'],
    ['GUIDE_KEY','根据歌曲需要选择演奏调。调整后，吹几个音确认调性。'],
    ['GUIDE_RECORD','点击开始录音。结束后，在我的录制中查看文件。']
  ].map(([id,text])=>({id,text}));
  const catalog=[...(window.FENGYIN_VOICE_CATALOG?.items || []),...updates];
  const clips=new Map(catalog.map(x=>[x.id,x]));
  const backend=window.__JUCE__?.backend;
  let enabled=localStorage.getItem('fy-voice-enabled')==='yes';
  let toneEnabled=localStorage.getItem('fy-voice-tones')!=='no';
  let volume=Number(localStorage.getItem('fy-voice-volume') || .45);
  let token=0, pending=null, state={}, connected=null, connectionTimer, noToneAt=0;
  let guiding=false, step=-1, guideTimer, countdown=false, learnedName='', wasCompact=false, videoReady=false;
  const emit=(name,value={})=>backend?.emitEvent(name,value);
  const busy=()=>!!state.recording || !!state.screenRecordingBusy || Number(state.breath)>0.015 || videoWantsPlaying || !video.paused;
  function stop(){emit('stopVoice');if(pending){clearTimeout(pending.timeout);pending.resolve(false);pending=null;}}
  function say(id,{force=false,completion=false,tone=false}={}){
    // Reuse the reviewed audio-only clip for this legacy step. The updated
    // recording step below explains the current audio/screen workflow in text.
    if(id==='GUIDE_RECORD')id='G010';
    if(!(tone?toneEnabled:enabled) || !clips.has(id) || (busy()&&!force) || state.recording || state.screenRecordingBusy)return Promise.resolve(false);
    stop();const request=++token;
    return new Promise(resolve=>{
      pending={request,resolve,completion,timeout:setTimeout(()=>{stop();},45000)};
      if(!backend){stop();return;}
      emit('playVoice',{id,token:request,volume});
    });
  }
  backend?.addEventListener('voiceFinished',result=>{
    const n=Number(result);if(!pending || Math.abs(n)!==pending.request)return;
    clearTimeout(pending.timeout);pending.resolve(n>0);pending=null;
  });
  function speakText(text){const x=catalog.find(x=>x.text.replace(/[。～]/g,'')===text.replace(/[。～]/g,''));return x?say(x.id):Promise.resolve(false);}
  const controlName=name=>name.split('／')[0].replace(/功能键\s*1/,'功能键一').replace(/功能键\s*2/,'功能键二').replace(/功能键\s*3/,'功能键三');
  window.fengyinVoice={say,stop,speakText,
    learning(name){learnedName=controlName(name);say(/咬嘴|颤音|揉弦/.test(name)?'V047':'NEW_MAP_START');},
    cleared(){say('NEW_MAP_CLEAR');},
    tone(name){if(!toneEnabled)return;const media=Object.values(window.FENGYIN_TONE_MEDIA||{}).find(t=>t.name===name);const spoken=name.replace('次中萨','次中音萨克斯').replace('上低萨','上低音萨克斯').replace('高萨','高音萨克斯').replace('中萨','中音萨克斯').replaceAll('-', '，').replaceAll('*','');const match=catalog.find(x=>x.text.replace(/[。～]/g,'')===spoken);return say(clips.has(media?.voiceId)?media.voiceId:(match?.id || 'NEW_TONE_GENERIC'),{tone:true});},
    recordingPreferences(){return {announce:enabled,volume};}
  };
  const style=document.createElement('style');style.textContent=`
  .voice-settings{margin-top:20px;display:flex;gap:16px;align-items:center;flex-wrap:wrap}.voice-settings label{display:flex;gap:8px;align-items:center}
  #fy-guide{position:fixed;z-index:20010;width:min(390px,calc(100vw - 32px));box-sizing:border-box;padding:20px;border-radius:18px;background:var(--bg,#091522);color:var(--text,#f4f8ff);box-shadow:0 16px 50px #0008;border:1px solid var(--accent,#53d8eb)}
  #fy-guide h3{margin:0 0 10px}#fy-guide p{line-height:1.7;margin:0 0 14px}#fy-guide button{margin:4px;padding:9px 12px;min-height:40px;cursor:pointer;border:1px solid var(--line,#536374);border-radius:9px;background:var(--accent-soft,#163a49);color:var(--text,#f4f8ff);font:inherit}#fy-guide button:disabled{opacity:.4;cursor:default}#fy-guide button[data-guide=next],#fy-guide-yes{border-color:var(--accent,#53d8eb);color:var(--accent,#53d8eb)}#fy-guide small{display:block;margin-bottom:7px;color:var(--muted,#a8bac9)}
  #fy-guide-spot{position:fixed;z-index:20008;border-radius:12px;border:2px solid var(--accent,#53d8eb);box-shadow:0 0 0 9999px #0008;pointer-events:none;transition:left .18s,top .18s,width .18s,height .18s}
  #fy-guide-hand{position:fixed;z-index:20009;pointer-events:none;font-size:36px;animation:fy-point 1.2s ease-in-out infinite;filter:drop-shadow(0 2px 3px #0008)}
  @keyframes fy-point{50%{transform:translateY(-6px)}}@media(prefers-reduced-motion:reduce){#fy-guide-hand{animation:none}#fy-guide-spot{transition:none}}
  #fy-guide[hidden],#fy-guide-spot[hidden],#fy-guide-hand[hidden]{display:none}
  `;document.head.append(style);
  const settings=document.createElement('div');settings.className='voice-settings';
  settings.innerHTML='<label><input type="checkbox" id="fy-voice-enabled" role="switch">语音提示</label><label><input type="checkbox" id="fy-voice-tones" role="switch">音色播报</label><label hidden>提示音量<input type="range" id="fy-voice-volume" min="0" max="70" aria-label="语音音量"></label><button id="fy-guide-restart" hidden>首次演奏引导</button><button id="fy-guide-advanced" hidden>进阶功能引导</button>';
  document.querySelector('#advanced-voice').append(settings);
  const guideEntry=document.createElement('button');
  guideEntry.id='fy-guide-open';guideEntry.type='button';guideEntry.textContent='新手指引';
  document.querySelector('#advanced-studio').prepend(guideEntry);
  document.querySelector('#fy-voice-enabled').checked=enabled;document.querySelector('#fy-voice-tones').checked=toneEnabled;
  document.querySelector('#fy-voice-volume').value=Math.round(volume*100);
  document.querySelector('#fy-voice-enabled').onchange=e=>{enabled=e.target.checked;localStorage.setItem('fy-voice-enabled',enabled?'yes':'no');if(enabled)say('V072');else stop();};
  document.querySelector('#fy-voice-tones').onchange=e=>{toneEnabled=e.target.checked;localStorage.setItem('fy-voice-tones',toneEnabled?'yes':'no');say(toneEnabled?'V076':'V077');};
  document.querySelector('#fy-voice-volume').oninput=e=>{volume=Number(e.target.value)/100;localStorage.setItem('fy-voice-volume',String(volume));};
  const panel=document.createElement('section');panel.id='fy-guide';panel.hidden=true;panel.setAttribute('role','dialog');panel.setAttribute('aria-label','首次使用引导');
  const spot=document.createElement('div');spot.id='fy-guide-spot';spot.hidden=true;
  const hand=document.createElement('div');hand.id='fy-guide-hand';hand.textContent='👆';hand.hidden=true;hand.setAttribute('aria-hidden','true');
  document.body.append(spot,hand,panel);
  const basicSteps=[
    ['连接电吹管','V002','.sidebar-status','play',null,'connected'],
    ['确认声音输出','GUIDE_OUTPUT','.audio-output-control','audio'],
    ['匹配气息','GUIDE_MATCH','#adapter-match','wind',null,'matched'],
    ['选择音色','V011','#preset-grid','sounds',null,'loaded'],
    ['试着吹奏','GUIDE_TRY','#sound-name','play',null,'played'],
  ];
  const advancedSteps=[
    ['技巧控制与映射','GUIDE_TECH','#adapter-technique-grid','wind'],
    ['乐曲速度','MT_GUIDE','#music-tempo','play'],
    ['演奏调','GUIDE_KEY','#transpose-key','play'],
    ['弯音范围','G004','#bend-range-button','play'],
    ['视频伴奏','V035','#choose-video','play'],
    ['录音与录屏',null,'.record-control','play',null,null,'点击“开始录制”，选择录音或录屏。风吟立即准备，准备好后播放“滴”声并开始录制；准备中可取消。再次点击停止后自动保存，按钮显示“正在保存，请稍等”。保存期间暂时不能吹奏，完成后恢复。'],
    ['我的录制',null,'#recording-manager','play',null,null,'点击“我的录制”查看文件。录音保存为 WAV；录屏自动保存 MP4，同时在录音文件夹保留同名无损 WAV，无需额外操作。临时文件单独存放，成功保存后自动清理。若保存失败，请按提示恢复保存。'],
    ['精简模式','V030','#window-mode-toggle','play'],
    ['更换主题','G013','#theme-button','play']
  ];
  const completeSteps=[...basicSteps,...advancedSteps,
    ['播放与进度',null,'.video-controls','play',null,null,'加载视频后，可播放、暂停或拖动进度。选择视频可更换伴奏；全屏按钮切换画面显示。'],
    ['伴奏音量',null,'#video-volume','play',null,null,'拖动视频音量调节伴奏响度。伴奏音量与吹奏音量分别控制。'],
    ['混响',null,'#performance-reverb','play',null,null,'调节乐器的混响量，伴奏保持原有声音。'],
    ['吹奏音量',null,'#master-volume','play',null,null,'调节吹奏音量，与伴奏取得合适的平衡。'],
    ['声音设置',null,'#page-audio','settings',null,null,'在高级设置中选择声音模式、输出设备、采样率和缓冲区，并查看预计延迟。修改会保存；如出现声音异常，查看页面的错误提示。'],
    ['授权与激活',null,'.activation','settings',null,null,'这里查看激活状态、复制本机识别码。未激活时，可输入激活码并点击激活。'],
    ['语音与音色播报',null,'#advanced-voice','settings',null,null,'语音提示和音色播报可以分别开关。录制开始的“滴”声用于确认开始时间。'],
    ['调音师工作台',null,'#studio-entry','settings',null,null,'调音师入口需要密码。进入后可创建或编辑音色方案、选择音源、调整效果链并保存或导出方案；导入方案后也可继续编辑。'],
    ['随时重新查看',null,'#fy-guide-open','settings',null,null,'以后可在高级设置点击“新手指引”重新查看。每一步都可以返回、跳过或结束；引导不会替你录制、激活或修改调音参数。'],
    ['可以开始演奏了','V012','.nav-item[data-page="play"]','play']];
  let steps=basicSteps,trialPerformed=false;
  function updateGuideProgress(){
    if(!guiding)return;
    const guard=steps[step]?.[5];
    const allowed=!guard || (guard==='connected'&&state.deviceConnected)
      || (guard==='matched'&&(state.deviceMatched||state.breathMatched))
      || (guard==='loaded'&&state.pluginLoaded) || (guard==='played'&&trialPerformed);
    const next=panel.querySelector('[data-guide=next]');if(next)next.disabled=!allowed;
  }
  function place(){
    if(!guiding)return;
    const target=document.querySelector(steps[step][2]);const r=target?.getBoundingClientRect();
    const visible=r && r.width>0 && r.height>0 && r.bottom>0 && r.top<innerHeight;
    spot.hidden=hand.hidden=!visible;
    if(visible){const x=Math.max(2,r.left-5),y=Math.max(2,r.top-5);Object.assign(spot.style,{left:x+'px',top:y+'px',width:Math.min(r.width+10,innerWidth-x-4)+'px',height:Math.min(r.height+10,innerHeight-y-4)+'px'});hand.style.left=Math.min(innerWidth-45,r.left+r.width*.5)+'px';hand.style.top=Math.min(innerHeight-45,r.bottom-8)+'px';}
    const width=Math.min(390,innerWidth-32);const h=panel.offsetHeight;
    let left=visible && r.right+width+30<innerWidth?r.right+20:Math.max(16,innerWidth-width-16);
    let top=visible && r.bottom+h+30<innerHeight?r.bottom+20:16;
    if(visible && r.left<left+width && r.right>left && r.top<top+h && r.bottom>top)left=16;
    Object.assign(panel.style,{left:left+'px',top:Math.max(8,Math.min(top,innerHeight-h-8))+'px'});
  }
  function end(){guiding=false;clearInterval(guideTimer);stop();panel.hidden=spot.hidden=hand.hidden=true;localStorage.setItem('fy-guide-seen','yes');}
  function show(index){
    if(index>=steps.length){end();return;}step=Math.max(0,index);stop();closeQuickTones();
    const [title,id,selector,page,mode]=steps[step];
    if(Number(window.fengyinTechniqueLearning)>=0){nativeEvent('cancelTechniqueLearn');window.fengyinTechniqueLearning=-1;}
    for(const id of ['technique-learning-dialog','technique-dialog','transpose-dialog','bend-range-dialog']){const dialog=document.getElementById(id);if(dialog)dialog.hidden=true;}
    if(compactMode && page!=='play'){nativeEvent('setCompactMode',{compact:false});applyWindowMode(false);}
    showPage(page);
    trialPerformed=false;
    panel.innerHTML=`<small>${step+1} / ${steps.length}</small><h3></h3><p></p><button data-guide="back">上一步</button><button data-guide="repeat">重听</button><button data-guide="next">${step===steps.length-1?'完成':'下一步'}</button><button data-guide="skip">跳过</button><button data-guide="end">结束引导</button>`;
    panel.querySelector('h3').textContent=title;panel.querySelector('p').textContent=steps[step][6] || clips.get(id)?.text || '';
    panel.querySelector('[data-guide=repeat]').disabled=!clips.has(id);
    panel.querySelector('[data-guide=back]').disabled=step===0;panel.hidden=false;
    document.querySelector(selector)?.scrollIntoView({block:'nearest'});place();updateGuideProgress();say(id);
  }
  function start(kind='all'){if(busy())return toast('请先停止吹奏、伴奏和录音，再开始引导');steps=kind==='advanced'?advancedSteps:kind==='basic'?basicSteps:completeSteps;guiding=true;panel.style.right=panel.style.bottom='auto';show(0);clearInterval(guideTimer);guideTimer=setInterval(place,150);}
  guideEntry.onclick=()=>start();
  panel.onclick=e=>{const action=e.target.dataset.guide;if(action==='next'||action==='skip')show(step+1);if(action==='back')show(step-1);if(action==='repeat')say(steps[step][1]);if(action==='end')end();};
  document.querySelector('#fy-guide-restart').onclick=()=>start();
  document.querySelector('#fy-guide-advanced').onclick=()=>start('advanced');
  window.fengyinVoice.guide={start,end,get active(){return guiding;},get step(){return step;}};
  document.addEventListener('keydown',e=>{if(e.key==='Escape' && guiding){end();e.stopImmediatePropagation();}},true);
  backend?.addEventListener('backendState',next=>{
    state=next;
    if(guiding&&steps[step]?.[5]==='played'&&Number(next.breath)>.02&&(next.noteReceived||Number(next.note)>=0))trialPerformed=true;
    updateGuideProgress();
    if(pending && !countdown && (state.recording || state.screenRecordingBusy || (busy()&&!pending.completion)))stop();
    const value=!!next.deviceConnected;
    if(value!==connected){const previous=connected;connected=value;clearTimeout(connectionTimer);if(value||previous===true)connectionTimer=setTimeout(()=>{if(!guiding)say(value?'V003':'V060');},900);}
    if(!next.pluginLoaded && Number(next.breath)>.03 && Date.now()-noToneAt>20000){noToneAt=Date.now();setTimeout(()=>{if(!state.pluginLoaded)say('V079');},1800);}
  });
  function announceSaved(ok){
    // Terminal save events are emitted only after the native recording gate is
    // released. Clear the preceding UI snapshot before playing the completion.
    state={...state,recording:false,screenRecordingBusy:false};
    if(ok)say('REC_SAVED',{force:true,completion:true});
  }
  backend?.addEventListener('recordingResult',result=>announceSaved(result?.success===true));
  // Legacy non-Windows audio recorder; Windows emits recordingResult only.
  backend?.addEventListener('recordingSaved',ok=>announceSaved(ok===true));
  backend?.addEventListener('windowModeChanged',compact=>{if(!compact && wasCompact && !guiding)say('V032');wasCompact=!!compact;place();});
  backend?.addEventListener('videoAudioState',result=>{if(result?.ready&&!videoReady)say('V036');videoReady=!!result?.ready;});
  backend?.addEventListener('techniqueLearnResult',result=>{if(result.success)say('NEW_MAP_OK');else say('V052');});
  backend?.addEventListener('breathMatchResult',result=>{if(!result.success)say('V007');});
  backend?.addEventListener('bendRangeResult',result=>{if(result.success && result.value)say(`V027_${String(result.value).padStart(2,'0')}`);});
  document.querySelector('#cancel-technique-learning')?.addEventListener('click',()=>say('NEW_MAP_CANCEL'));
  const offer=setInterval(()=>{
    if(localStorage.getItem('fy-guide-seen')){clearInterval(offer);return;}
    if(busy() || !document.querySelector('#license-lock').hidden)return;
    clearInterval(offer);
    panel.hidden=false;panel.style.right='20px';panel.style.bottom='20px';
    panel.innerHTML='<h3>欢迎使用风吟</h3><p>是否开启语音，跟随指引熟悉常用功能？可以随时跳过。</p><button id="fy-guide-yes">开启语音并开始</button><button id="fy-guide-no">暂不引导</button>';
    document.querySelector('#fy-guide-yes').onclick=()=>{enabled=true;localStorage.setItem('fy-voice-enabled','yes');document.querySelector('#fy-voice-enabled').checked=true;panel.style.right=panel.style.bottom='auto';start();};
    document.querySelector('#fy-guide-no').onclick=()=>{end();};
  },1800);
  window.addEventListener('beforeunload',stop);
})();
