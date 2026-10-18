var __t=typeof window!=='undefined'&&window.epaperI18n?window.epaperI18n.t:(s=>s);
(()=>{
  const el=id=>document.getElementById(id);
  let value=null,connected=false,busy=false,pending=null,soundKey='',periodKey='',soundDirty=false,periodDirty=false,counter=0,token='',watchTest=0;
  const states={disabled:__t('已关闭'),idle:__t('等待下一次报时'),paused:__t('当前时段暂停'),muted:__t('音量为0'),preparing:__t('正在准备'),armed:__t('等待倒计时'),playing:__t('正在播报'),beeping:__t('倒计时滴声'),prefix:__t('正在播报：现在时刻'),'time-phrase':__t('正在播报时间'),'environment-phrase':__t('正在播报温湿度'),testing:__t('正在试听'),'resources-missing':__t('小乐音库不可用'),'config-invalid':__t('报时配置无效'),'time-invalid':__t('设备时间无效'),'all-day-paused':__t('已启用的报时点全部暂停'),provisioning:__t('配网期间暂停'),unavailable:__t('服务不可用')};
  function sound(){return {enabled:el('sound-enabled').checked,minutes:[...(el('sound-hourly').checked?[0]:[]),...(el('sound-half').checked?[30]:[])],volume_percent:Number(el('sound-volume').value)}}
  function savedSound(){return {enabled:value.enabled,minutes:[...(value.hourly?[0]:[]),...(value.half_hourly?[30]:[])],volume_percent:value.volume_percent}}
  function periods(){return [...el('period-list').children].map(row=>({id:row.dataset.id,name:row.querySelector('[data-name]').value,enabled:row.querySelector('[data-enabled]').checked,start:row.querySelector('[data-start]').value,end:row.querySelector('[data-end]').value}))}
  function range(v){return v.start===v.end?__t('起止时间不能相同'):v.start<v.end?__t('当天 ')+v.start+'–'+v.end:__t('跨午夜 ')+v.start+__t('–次日 ')+v.end}
  function timeSelect(value){const select=document.createElement('select');for(let m=0;m<1440;m+=15){const text=String(Math.floor(m/60)).padStart(2,'0')+':'+String(m%60).padStart(2,'0');const option=document.createElement('option');option.value=option.textContent=text;select.append(option)}if(![...select.options].some(o=>o.value===value)){const option=document.createElement('option');option.value=value;option.textContent=value+__t('（已保存）');select.append(option)}select.value=value;return select}
  function addPeriod(v){
    const row=document.createElement('div');row.className='announcement-period';row.dataset.id=v.id;
    const heading=document.createElement('div');heading.className='announcement-period-heading';
    const name=document.createElement('input');name.type='text';name.maxLength=32;name.value=v.name;name.dataset.name='';name.setAttribute('aria-label',__t('时段名称'));
    const enabled=document.createElement('input');enabled.type='checkbox';enabled.checked=v.enabled;enabled.dataset.enabled='';enabled.setAttribute('aria-label',__t('启用 ')+v.name);
    const remove=document.createElement('button');remove.type='button';remove.className='secondary';remove.textContent=__t('删除');remove.onclick=()=>{row.remove();draft()};
    heading.append(enabled,name,remove);row.append(heading);
    const fields=document.createElement('div');fields.className='night-time-fields';
    for(const [key,title]of [['start',__t('开始时间')],['end',__t('结束时间')]]){const label=document.createElement('label');label.textContent=title;const select=timeSelect(v[key]);select.dataset[key]='';select.setAttribute('aria-label',title+' · '+v.name);label.append(select);fields.append(label)}
    const summary=document.createElement('p');summary.className='note';summary.dataset.summary='';row.append(fields,summary);row.oninput=row.onchange=draft;el('period-list').append(row);
  }
  function loadSound(){const v=savedSound();el('sound-enabled').checked=v.enabled;el('sound-hourly').checked=v.minutes.includes(0);el('sound-half').checked=v.minutes.includes(30);el('sound-volume').value=v.volume_percent;soundKey=JSON.stringify(v);soundDirty=false}
  function loadPeriods(){el('period-list').replaceChildren();for(const v of value.pause_periods||[])addPeriod(v);periodKey=JSON.stringify(value.pause_periods||[]);periodDirty=false}
  function validPeriods(v){return v.length<=8&&v.every(p=>p.name.trim()&&new TextEncoder().encode(p.name).length<=96&&/^([01]\d|2[0-3]):[0-5]\d$/.test(p.start)&&/^([01]\d|2[0-3]):[0-5]\d$/.test(p.end)&&p.start!==p.end)}
  function draft(){
    if(!value)return;soundDirty=JSON.stringify(sound())!==soundKey;periodDirty=JSON.stringify(periods())!==periodKey;
    const locked=!connected||busy||!!pending||!!watchTest||latest.control.busy||value.busy;
    for(const id of ['sound-settings','period-settings'])for(const control of el(id).querySelectorAll('input,select,button'))control.disabled=locked;
    for(const id of ['test-hour','test-half'])el(id).title=value.test_available===false?__t('自动报时临近或正在播报，试听暂不可用'):__t('立即试听');
    const s=sound();el('sound-volume-number').textContent=s.volume_percent+'%';el('sound-state').textContent=soundDirty?__t('尚未保存'):__t('已保存');el('period-state').textContent=periodDirty?__t('尚未保存'):__t('已保存');
    el('save-sound').disabled=locked||!soundDirty||!s.minutes.length||(s.enabled&&!value.ready);el('restore-sound').disabled=locked||!soundDirty;
    el('save-periods').disabled=locked||!periodDirty||!validPeriods(periods());el('restore-periods').disabled=locked||!periodDirty;el('add-period').disabled=locked||periods().length>=8;
    for(const id of ['open-sound','open-period'])el(id).disabled=locked;
    for(const id of ['test-hour','test-half'])el(id).disabled=locked||!value.ready||value.test_available===false||!s.volume_percent;
    
    for(const row of el('period-list').children){const v={start:row.querySelector('[data-start]').value,end:row.querySelector('[data-end]').value};row.querySelector('[data-summary]').textContent=range(v)}
  }
  async function send(kind,data){
    if(!connected||busy||pending||latest.control.busy||value.busy)return;busy=true;draft();saveMessage('announcement',kind==='test'?__t('正在请求试听…'):__t('正在保存…'),false,true);
    try{const url=kind==='sound'?'/api/announcement-settings':kind==='period'?'/api/announcement-pause-settings':'/api/announcement-test';const body=kind==='test'?data:{schema_version:1,...data};const response=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json','X-Album-Token':latest.control.token},body:JSON.stringify(body),signal:AbortSignal.timeout(5000)});if(!response.ok)throw Error(await response.text());const job=await response.json();pending={id:job.job_id,kind};}
    catch(error){saveMessage('announcement',__t('提交未确认：')+error.message,true)}finally{busy=false;draft();await refresh()}
  }
  window.updateAnnouncement=(s,online=true)=>{
    connected=online;
    if(s?.control?.token&&token!==s.control.token){if(token){pending=null;watchTest=0;soundKey=periodKey='';soundDirty=periodDirty=false;}token=s.control.token;}
    if(!s?.announcement){draft();return}value=s.announcement;
    if(pending&&s.control.job_id===pending.id&&['succeeded','failed'].includes(s.control.state)){
      const kind=pending.kind,ok=s.control.state==='succeeded';pending=null;
      if(ok){if(kind==='sound'){loadSound();el('sound-dialog').close()}else if(kind==='period'){loadPeriods();el('period-dialog').close()}}
      if(ok&&kind==='test'){watchTest=value.test_request_id;saveMessage('announcement',__t('正在试听…'),false,true)}else saveMessage('announcement',ok?__t('已保存到 SD 卡并应用。'):__t('操作失败：')+s.control.error,!ok);
    }
    if(pending&&s.control.job_id>pending.id){pending=null;saveMessage('announcement',__t('操作记录已更新。当前显示设备最新设置。'),true);}
    if(watchTest&&value.completed_test_id>=watchTest){watchTest=0;const ok=value.last_test_result==='succeeded',partial=value.last_test_result==='partial';saveMessage('announcement',ok?__t('试听已完成。'):partial?__t('时间播报完成，温湿度播报未完成。'):__t('试听未完成：')+(value.last_test_error||value.last_test_result),!ok);}
    if(!soundKey||(!soundDirty&&soundKey!==JSON.stringify(savedSound())))loadSound();if(!periodKey||(!periodDirty&&periodKey!==JSON.stringify(value.pause_periods||[])))loadPeriods();
    el('sound-status').textContent=states[value.phase]||value.phase;el('speech-engine-status').textContent=value.ready?__t('ESP-TTS · 小乐 · 语速 2'):__t('小乐音库不可用');
    el('sound-next').textContent=value.next_target_epoch?beijing(value.next_target_epoch):__t('未安排');el('sound-last').textContent=value.last_target_epoch?beijing(value.last_target_epoch):__t('暂无记录');
    el('sound-result').textContent=({none:__t('暂无记录'),succeeded:__t('已完成'),skipped:__t('已跳过'),error:__t('失败'),partial:__t('仅完成时间播报')})[value.last_result]||value.last_result;el('sound-error').textContent=value.error||'';
    el('sound-summary').textContent=value.enabled?(value.hourly&&value.half_hourly?__t('整点与半点'):value.hourly?__t('整点'):__t('半点'))+__t(' · 音量 ')+value.volume_percent+'%':__t('已关闭');
    const enabled=(value.pause_periods||[]).filter(p=>p.enabled);el('period-summary').textContent=enabled.length?__t('已启用 ')+enabled.length+__t(' 个暂停时段'):__t('未设置暂停时段');
    const list=enabled.map(p=>{const item=document.createElement('p');item.className='note';item.textContent=p.name+' · '+range(p);return item});el('period-summary-list').replaceChildren(...list);draft();
  };
  for(const [kind,id]of [['sound','sound-dialog'],['period','period-dialog']]){const dialog=el(id);el('open-'+kind).onclick=()=>{draft();dialog.showModal();el(kind+'-feedback').append(el('feedback'))};dialog.querySelector('.modal-close').onclick=()=>dialog.close();dialog.addEventListener('close',()=>document.body.append(el('feedback')))}
  el('sound-settings').oninput=el('sound-settings').onchange=draft;el('sound-settings').onsubmit=e=>{e.preventDefault();send('sound',sound())};el('period-settings').onsubmit=e=>{e.preventDefault();send('period',{pause_periods:periods()})};
  el('restore-sound').onclick=()=>{loadSound();draft()};el('restore-periods').onclick=()=>{loadPeriods();draft()};
  el('add-period').onclick=()=>{addPeriod({id:'period-'+Date.now().toString(36)+'-'+(++counter),name:__t('暂停时段'),enabled:true,start:'12:00',end:'14:00'});draft()};
  el('test-hour').onclick=()=>send('test',{kind:'hour',volume_percent:sound().volume_percent});el('test-half').onclick=()=>send('test',{kind:'half',volume_percent:sound().volume_percent});
  if(typeof latest!=='undefined'&&latest)window.updateAnnouncement(latest,available);
})();
