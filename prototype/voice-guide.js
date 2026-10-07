/* Offline, reviewed speech. No browser/cloud TTS in the installed application. */
(() => {
  const catalog=window.FENGYIN_VOICE_CATALOG?.items || [];
  const clips=new Map(catalog.map(x=>[x.id,x]));
  const backend=window.__JUCE__?.backend;
  let enabled=localStorage.getItem('fy-voice-enabled')==='yes';
  let toneEnabled=localStorage.getItem('fy-voice-tones')!=='no';
  let volume=Number(localStorage.getItem('fy-voice-volume') || .45);
  let token=0, pending=null, state={}, connected=null, connectionTimer, noToneAt=0;
  let guiding=false, step=-1, guideTimer, countdown=false, learnedName='', wasCompact=false, videoReady=false;
  const emit=(name,value={})=>backend?.emitEvent(name,value);
  const busy=()=>!!state.recording || Number(state.breath)>0.015 || !video.paused;
  function stop(){emit('stopVoice');if(pending){clearTimeout(pending.timeout);pending.resolve(false);pending=null;}}
  function say(id,{force=false}={}){
    if(!enabled || !clips.has(id) || (busy()&&!force) || state.recording)return Promise.resolve(false);
    stop();const request=++token;
    return new Promise(resolve=>{
      pending={request,resolve,timeout:setTimeout(()=>{stop();},45000)};
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
    learning(name){learnedName=controlName(name);say(name.includes('咬嘴')?'V047':name.includes('体感')?'V048':name.includes('摇杆')?'V049':'V050');},
    cleared(name){speakText(`已取消${controlName(name)}映射。`);},
    tone(name){if(!toneEnabled)return;const spoken=name.replace('次中萨','次中音萨克斯').replace('上低萨','上低音萨克斯').replace('高萨','高音萨克斯').replace('中萨','中音萨克斯').replaceAll('-', '，');return speakText(spoken);},
    async beforeRecording(){
      if(countdown)return false;if(!enabled)return true;
      countdown=true;const button=document.querySelector('#record');button.disabled=true;
      button.textContent='准备录音…';
      try{return await say('V041',{force:true});}
      finally{countdown=false;button.disabled=false;button.textContent='● 开始录音';}
    }
  };
  const style=document.createElement('style');style.textContent=`
  .voice-settings{margin-top:20px;display:flex;gap:16px;align-items:center;flex-wrap:wrap}.voice-settings label{display:flex;gap:8px;align-items:center}
  #fy-guide{position:fixed;z-index:20010;width:min(390px,calc(100vw - 32px));box-sizing:border-box;padding:20px;border-radius:18px;background:var(--panel,#122437);color:var(--text,#f4f8ff);box-shadow:0 16px 50px #0008;border:1px solid var(--accent,#53d8eb)}
  #fy-guide h3{margin:0 0 10px}#fy-guide p{line-height:1.7;margin:0 0 14px}#fy-guide button{margin:4px;padding:8px 12px;cursor:pointer}#fy-guide small{display:block;margin-bottom:7px}
  #fy-guide-spot{position:fixed;z-index:20008;border-radius:12px;border:2px solid var(--accent,#53d8eb);box-shadow:0 0 0 9999px #0008;pointer-events:none;transition:left .18s,top .18s,width .18s,height .18s}
  #fy-guide-hand{position:fixed;z-index:20009;pointer-events:none;font-size:36px;animation:fy-point 1.2s ease-in-out infinite;filter:drop-shadow(0 2px 3px #0008)}
  @keyframes fy-point{50%{transform:translateY(-6px)}}@media(prefers-reduced-motion:reduce){#fy-guide-hand{animation:none}#fy-guide-spot{transition:none}}
  #fy-guide[hidden],#fy-guide-spot[hidden],#fy-guide-hand[hidden]{display:none}
  `;document.head.append(style);
  const settings=document.createElement('div');settings.className='voice-settings';
  settings.innerHTML='<label><input type="checkbox" id="fy-voice-enabled">语音提示</label><label><input type="checkbox" id="fy-voice-tones">音色播报</label><label>提示音量<input type="range" id="fy-voice-volume" min="0" max="70" aria-label="语音音量"></label><button id="fy-guide-restart">重新开始引导</button>';
  document.querySelector('#page-settings .panel').append(settings);
  document.querySelector('#fy-voice-enabled').checked=enabled;document.querySelector('#fy-voice-tones').checked=toneEnabled;
  document.querySelector('#fy-voice-volume').value=Math.round(volume*100);
  document.querySelector('#fy-voice-enabled').onchange=e=>{enabled=e.target.checked;localStorage.setItem('fy-voice-enabled',enabled?'yes':'no');if(enabled)say('V072');else stop();};
  document.querySelector('#fy-voice-tones').onchange=e=>{toneEnabled=e.target.checked;localStorage.setItem('fy-voice-tones',toneEnabled?'yes':'no');say(toneEnabled?'V076':'V077');};
  document.querySelector('#fy-voice-volume').oninput=e=>{volume=Number(e.target.value)/100;localStorage.setItem('fy-voice-volume',String(volume));};
  const panel=document.createElement('section');panel.id='fy-guide';panel.hidden=true;panel.setAttribute('role','dialog');panel.setAttribute('aria-label','首次使用引导');
  const spot=document.createElement('div');spot.id='fy-guide-spot';spot.hidden=true;
  const hand=document.createElement('div');hand.id='fy-guide-hand';hand.textContent='👆';hand.hidden=true;hand.setAttribute('aria-hidden','true');
  document.body.append(spot,hand,panel);
  const steps=[
    ['欢迎使用风吟','V001','.nav-item[data-page="play"]','play'],
    ['连接电吹管','V002','.sidebar-status','play'],
    ['电吹管调性','V004','.sidebar-status','play'],
    ['检测气息','V005','#adapter-match','wind'],
    ['选择音色','V011','#preset-grid','sounds'],
    ['试着吹奏','G002','#sound-name','play'],
    ['演奏调','G003','#transpose-key','play'],
    ['弯音范围','G004','#bend-range-button','play'],
    ['演奏音量','V028','#master-volume','play'],
    ['混响','V029','#performance-reverb','play'],
    ['演奏技巧','V034','#technique-settings','play'],
    ['气息技巧','G005','#adapter-technique-grid','wind'],
    ['硬件映射','G006','#adapter-technique-grid','wind','hardware'],
    ['取消映射','G007','#adapter-technique-grid','wind','hardware'],
    ['视频伴奏','V035','#choose-video','play'],
    ['播放与暂停','G008','#main-play','play'],
    ['伴奏音量','G009','#video-volume','play'],
    ['开始录音','V040','#record','play'],
    ['停止并保存','G010','#record','play'],
    ['我的录音','G011','#recording-manager','play'],
    ['精简模式','V030','#window-mode-toggle','play'],
    ['更换主题','G013','#theme','play'],
    ['声音设置','G014','.audio-settings-grid','audio'],
    ['语音与重看引导','G015','.voice-settings','settings'],
    ['引导完成','V012','.nav-item[data-page="play"]','play']
  ];
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
    if(page==='wind'){document.querySelector(`[data-adapter-mode="${mode||'breath'}"]`)?.click();}
    panel.innerHTML=`<small>${step+1} / ${steps.length}</small><h3></h3><p></p><button data-guide="back">上一步</button><button data-guide="repeat">重听</button><button data-guide="next">${step===steps.length-1?'完成':'下一步'}</button><button data-guide="skip">跳过</button><button data-guide="end">结束引导</button>`;
    panel.querySelector('h3').textContent=title;panel.querySelector('p').textContent=clips.get(id)?.text || '';
    panel.querySelector('[data-guide=back]').disabled=step===0;panel.hidden=false;
    document.querySelector(selector)?.scrollIntoView({block:'nearest'});place();say(id);
  }
  function start(){if(busy())return toast('请先停止吹奏、伴奏和录音，再开始引导');guiding=true;show(0);clearInterval(guideTimer);guideTimer=setInterval(place,150);}
  panel.onclick=e=>{const action=e.target.dataset.guide;if(action==='next'||action==='skip')show(step+1);if(action==='back')show(step-1);if(action==='repeat')say(steps[step][1]);if(action==='end')end();};
  document.querySelector('#fy-guide-restart').onclick=start;
  window.fengyinVoice.guide={start,end,get active(){return guiding;},get step(){return step;}};
  document.addEventListener('keydown',e=>{if(e.key==='Escape' && guiding){end();e.stopImmediatePropagation();}},true);
  backend?.addEventListener('backendState',next=>{
    state=next;
    if(busy() && pending && !countdown)stop();
    const value=!!next.deviceConnected;
    if(value!==connected){const previous=connected;connected=value;clearTimeout(connectionTimer);if(value||previous===true)connectionTimer=setTimeout(()=>{if(!guiding)say(value?'V003':'V060');},900);}
    if(!next.pluginLoaded && Number(next.breath)>.03 && Date.now()-noToneAt>20000){noToneAt=Date.now();setTimeout(()=>{if(!state.pluginLoaded)say('V079');},1800);}
  });
  backend?.addEventListener('recordingSaved',ok=>{state={...state,recording:false};if(ok)say('V042');});
  backend?.addEventListener('windowModeChanged',compact=>{if(!compact && wasCompact && !guiding)say('V032');wasCompact=!!compact;place();});
  backend?.addEventListener('videoAudioState',result=>{if(result?.ready&&!videoReady)say('V036');videoReady=!!result?.ready;});
  backend?.addEventListener('techniqueLearnResult',result=>{if(result.success)speakText(`${learnedName}已设置好。`);else say('V052');});
  backend?.addEventListener('breathMatchResult',result=>{if(!result.success)say('V007');});
  backend?.addEventListener('bendRangeResult',result=>{if(result.success && result.value)say(`V027_${String(result.value).padStart(2,'0')}`);});
  document.querySelector('#cancel-technique-learning')?.addEventListener('click',()=>say('V055'));
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
