var __t=typeof window!=='undefined'&&window.epaperI18n?window.epaperI18n.t:(s=>s);
/* Shared navigation and dialog behavior; device and SD API logic stays in each workspace. */
(()=>{'use strict';
const page=document.body.dataset.page;
const dateTimeFormat=new Intl.DateTimeFormat('en-GB',{timeZone:'Asia/Shanghai',year:'numeric',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',second:'2-digit',hourCycle:'h23'});
window.dateTime=value=>{
  const date=new Date(value);
  if(value==null||!Number.isFinite(date.getTime()))return '—';
  const parts=Object.fromEntries(dateTimeFormat.formatToParts(date).map(p=>[p.type,p.value]));
  return parts.year+'-'+parts.month+'-'+parts.day+' '+parts.hour+':'+parts.minute+':'+parts.second;
};
window.formatDuration=function(n){
  if(n==null||!Number.isFinite(n)||n<0)return '—';
  let remaining=Math.floor(n),parts=[];
  for(const [size,unit] of [[86400,__t('天')],[3600,__t('小时')],[60,__t('分钟')],[1,__t('秒')]]){
    const value=Math.floor(remaining/size);
    if(value)parts.push(value+unit);
    remaining%=size;
  }
  return parts.join(window.epaperI18n?.language==='en'?' ':'')||__t('0秒');
};
const clockNode=document.getElementById('device-clock');
let clockEpoch=null,clockSample=0;
let countdowns=null,countdownSample=0,uptimeSeconds=null;
function tickDurations(){
  const elapsed=Math.max(0,Math.floor((performance.now()-countdownSample)/1000));
  for(const node of document.querySelectorAll('[data-countdown]')){
    const value=countdowns?.[node.dataset.countdown];
    node.textContent=!value?'—':!value.active?__t('未安排'):Number.isFinite(value.seconds)&&value.seconds>=0?window.formatDuration(Math.max(0,Math.floor(value.seconds)-elapsed)):'—';
  }
  for(const node of document.querySelectorAll('[data-uptime]'))node.textContent=uptimeSeconds==null?'—':window.formatDuration(uptimeSeconds+elapsed);
}
function updateDurations(s){
  uptimeSeconds=Number.isFinite(s.device?.uptime_seconds)&&s.device.uptime_seconds>=0?s.device.uptime_seconds:null;
  countdownSample=performance.now()-Math.max(0,Number(s.rtc.age_ms)||0);
  countdowns={announcement:{active:!!s.announcement?.next_target_epoch,seconds:s.announcement?.next_seconds||0},photo:{active:s.album.enabled&&!s.album.automatic_stopped,seconds:s.album.next_seconds},sync:{active:s.network.sync_enabled,seconds:s.network.next_sync_seconds}};
  tickDurations();
}
function tickClock(){
  const epoch=clockEpoch==null?null:clockEpoch+performance.now()-clockSample;
  const text=epoch==null?'—':window.dateTime(epoch);
  const iso=epoch==null?null:new Date(epoch).toISOString();
  for(const node of document.querySelectorAll('[data-device-clock]')){
    node.textContent=text;
    if(iso==null)node.removeAttribute('datetime');else node.dateTime=iso;
  }
  tickDurations();
}
function updateClock(rtc){
  const epoch=rtc?.valid&&rtc.utc?Date.parse(rtc.calendar):NaN;
  clockEpoch=Number.isFinite(epoch)?epoch+Math.max(0,Number(rtc.age_ms)||0):null;clockSample=performance.now();
  clockNode.title=clockEpoch==null?__t('设备 RTC 时间无效'):__t('设备 RTC · 设备时间');tickClock();
}
function clockOffline(){uptimeSeconds=null;countdowns=null;clockEpoch=null;clockNode.title=__t('设备离线');tickClock()}
setInterval(tickClock,1000);
window.renderFacts=(node,text)=>{const lines=text.match(/[^。]+。?/g)||[];node.replaceChildren(...lines.map(line=>{const span=document.createElement('span');span.className='fact-line';span.textContent=line;return span}))};
for(const node of document.querySelectorAll('.note:not(.draft-note),footer:not(.side-footer)'))if(node.textContent.trim()&&!node.querySelector('.fact-line'))window.renderFacts(node,node.textContent);
// Dialogs close only through explicit controls or completed actions.
for(const dialog of document.querySelectorAll('dialog'))dialog.addEventListener('cancel',e=>e.preventDefault());
const views=page==='albums'?{photos:[__t('相册管理'),__t('照片库'),__t('SD 卡相册与照片文件')]}:{overview:[__t('设备管理'),__t('设备概览'),__t('设备信息、播放进度与 SD 卡容量')],network:[__t('设备管理'),__t('网络与时间'),__t('Wi-Fi 连接、校时服务器、同步记录与 RTC 时间')],environment:[__t('设备管理'),__t('环境与供电'),__t('温湿度、电池和 USB 供电状态')],sound:[__t('设备管理'),__t('声音与报时'),__t('整点、半点、温湿度与暂停播报设置')],playback:[__t('相册管理'),__t('播放设置'),__t('已保存的播放相册与停留间隔')]};
function closeMenu(){document.body.classList.remove('menu-open');document.getElementById('menu-toggle').setAttribute('aria-expanded','false')}
function apply(close=false){const fallback=page==='albums'?'photos':'overview',key=Object.hasOwn(views,location.hash.slice(1).split('/')[0])?location.hash.slice(1).split('/')[0]:fallback;const [group,title,description]=views[key];document.body.dataset.view=key;document.getElementById('view-title').textContent=title;document.getElementById('view-description').textContent=description;document.getElementById('crumb-group').textContent=group;document.getElementById('crumb-view').textContent=title;document.title=title+__t(' · 墨水相册');for(const node of document.querySelectorAll('[data-view]')){if(node===document.body)continue;node.hidden=!node.dataset.view.split(' ').includes(key)}for(const a of document.querySelectorAll('nav a')){const u=new URL(a.href,location.href),active=u.pathname===location.pathname&&u.hash.slice(1)===key;if(active){a.setAttribute('aria-current','page');if(close)a.closest('details').open=true}else a.removeAttribute('aria-current')}if(close)closeMenu()}
window.applyWorkspace=apply;window.addEventListener('hashchange',()=>apply(true));
document.getElementById('menu-toggle').onclick=()=>{const open=document.body.classList.toggle('menu-open');document.getElementById('menu-toggle').setAttribute('aria-expanded',String(open))};document.getElementById('menu-backdrop').onclick=closeMenu;document.addEventListener('keydown',e=>{if(e.key==='Escape'&&!document.querySelector('dialog[open]'))closeMenu()});
document.querySelector('nav').addEventListener('click',e=>{const a=e.target.closest('a');if(!a)return;if(document.body.dataset.operation==='busy'&&new URL(a.href).pathname!==location.pathname){e.preventDefault();document.getElementById('nav-feedback').textContent=__t('操作尚未完成，请等待后再切换工作区。')}else document.getElementById('nav-feedback').textContent=''});
let actionStatus=null,actionSending=false,actionJob=0;
function sidebarMessage(text){const node=document.getElementById('sidebar-next');node.textContent=!text||text===__t('换图完成')?__t('切换下一张'):text.startsWith(__t('换图失败'))?__t('换图失败，重试'):text;node.title=text||__t('切换下一张')}
window.sidebarOffline=()=>{clockOffline();$('sidebar-next').disabled=true;$('sidebar-album').textContent='—';$('sidebar-album').title='';$('sidebar-photo').textContent='—';$('sidebar-photo').title='';$('sidebar-photo').disabled=true;sidebarMessage(__t('设备离线'))};
window.renderSidebar=s=>{updateClock(s.rtc);updateDurations(s);if(actionStatus&&actionStatus.control.token!==s.control.token){actionJob=0;window.photoPreview?.close()}actionStatus=s;const albumName=s.album.options?.find(a=>a.id===s.album.current_album)?.directory||s.album.current_album;$('sidebar-album').textContent=albumName||'—';$('sidebar-album').title=albumName||'';const photo=s.album.current_photo?.split('/').pop()||'';$('sidebar-photo').textContent=photo||'—';$('sidebar-photo').title=photo;$('sidebar-photo').setAttribute('aria-label',photo?__t('预览当前照片 ')+photo:__t('预览当前照片'));updatePreviewButton();const c=s.control;$('sidebar-next').disabled=actionSending||c.busy||actionJob>c.job_id||document.body.dataset.operation==='busy'||!s.album.ready;if(actionJob&&c.job_id===actionJob)sidebarMessage(c.state==='succeeded'?__t('换图完成'):c.state==='failed'?__t('换图失败：')+c.error:c.state==='queued'&&s.announcement?.busy?__t('报时后切换照片…'):__t('正在换图，请等待…'));else sidebarMessage(actionSending?__t('正在请求换图…'):actionJob>c.job_id?__t('正在换图，请等待…'):c.busy?__t('设备忙碌'):'')};
const $=id=>document.getElementById(id);$('sidebar-next').onclick=async()=>{if(!actionStatus||actionSending||actionStatus.control.busy||document.body.dataset.operation==='busy')return;actionSending=true;document.body.dataset.operation='busy';$('sidebar-next').disabled=true;sidebarMessage(__t('正在请求换图…'));try{const r=await fetch('/api/next',{method:'POST',headers:{'Content-Type':'application/json','X-Album-Token':actionStatus.control.token},body:'{}',signal:AbortSignal.timeout(4000)});if(!r.ok)throw Error('HTTP '+r.status);actionJob=(await r.json()).job_id;sidebarMessage(__t('正在换图，请等待…'))}catch(e){sidebarMessage(__t('请求未确认，请刷新核对。'))}finally{actionSending=false;document.body.dataset.operation=''}};
const previewDialog=$('preview'),previewImage=$('preview-image'),previewStatus=$('preview-status');
let previewUrl='',previewRequest=null,previewGeneration=0,previewPending=false,previewAlbum='',previewNames=[],previewIndex=-1;
function updatePreviewNav(){const blocked=previewPending||!actionStatus||actionStatus.control.busy||document.body.dataset.operation==='busy';$('preview-previous').disabled=blocked||previewIndex<=0;$('preview-next').disabled=blocked||previewIndex<0||previewIndex>=previewNames.length-1;$('preview-position').textContent=previewIndex>=0?(previewIndex+1)+' / '+previewNames.length:''}
function updatePreviewButton(){const s=actionStatus;$('sidebar-photo').disabled=previewPending||!s||s.control.busy||actionSending||actionJob>s.control.job_id||document.body.dataset.operation==='busy'||!s.album.current_album||!s.album.current_photo;updatePreviewNav()}
function clearPreviewImage(){previewImage.removeAttribute('src');previewImage.hidden=true;if(previewUrl)URL.revokeObjectURL(previewUrl);previewUrl=''}
function resetPreview(){previewGeneration++;previewRequest?.abort();previewRequest=null;previewPending=false;previewAlbum='';previewNames=[];previewIndex=-1;clearPreviewImage();previewStatus.hidden=true;updatePreviewButton()}
function closePreview(){resetPreview();if(previewDialog.open)previewDialog.close()}
function previewName(name){$('preview-name').textContent=name;$('preview-name').title=name}
function paintPreview(blob,name){clearPreviewImage();previewName(name);previewUrl=URL.createObjectURL(blob);previewImage.src=previewUrl;previewImage.hidden=false;previewStatus.textContent=__t('正在读取照片…');previewStatus.hidden=false;if(!previewDialog.open)previewDialog.showModal()}
async function previewFetch(path,signal){const response=await fetch(path,{headers:{'X-Album-Token':actionStatus.control.token},cache:'no-store',signal:AbortSignal.any([signal,AbortSignal.timeout(65000)])});if(!response.ok)throw Error(response.status===409?__t('设备忙碌，请稍后重试。'):response.status===404?__t('照片不存在。'):__t('照片读取失败（HTTP ')+response.status+__t('）。'));return response}
function previewError(error){previewStatus.textContent=error.name==='TimeoutError'?__t('照片读取超时。'):error.message;previewStatus.hidden=false}
async function openPreview(album,name,blob){
  resetPreview();previewAlbum=album;previewPending=true;const generation=previewGeneration;previewRequest=new AbortController();const signal=previewRequest.signal;previewName(name);previewStatus.textContent=__t('正在读取照片…');previewStatus.hidden=false;previewDialog.showModal();updatePreviewButton();
  try{
    if(!blob)blob=await(await previewFetch('/api/photo?'+new URLSearchParams({album,name}),signal)).blob();
    if(generation!==previewGeneration||!previewDialog.open)return;paintPreview(blob,name);
    const names=[];let offset=0;
    do{const page=await(await previewFetch('/api/files?'+new URLSearchParams({album,offset,limit:100}),signal)).json();names.push(...page.files.map(f=>f.name));if(page.next_offset===null)break;if(page.next_offset<=offset)throw Error(__t('相册列表无效。'));offset=page.next_offset}while(offset<2048);
    if(generation!==previewGeneration||!previewDialog.open)return;previewNames=names;previewIndex=names.indexOf(name);
  }catch(error){if(generation===previewGeneration&&previewDialog.open)previewError(error)}
  finally{if(generation===previewGeneration){previewPending=false;previewRequest=null;updatePreviewButton()}}
}
async function browsePreview(step){
  updatePreviewNav();if((step<0?$('preview-previous'):$('preview-next')).disabled)return;
  const index=previewIndex+step,name=previewNames[index],album=previewAlbum,generation=++previewGeneration;previewPending=true;previewRequest=new AbortController();const signal=previewRequest.signal;previewImage.hidden=true;previewName(name);previewStatus.textContent=__t('正在读取照片…');previewStatus.hidden=false;updatePreviewButton();
  try{const blob=await(await previewFetch('/api/photo?'+new URLSearchParams({album,name}),signal)).blob();if(generation!==previewGeneration||!previewDialog.open)return;previewIndex=index;paintPreview(blob,name)}
  catch(error){if(generation===previewGeneration&&previewDialog.open){previewName(previewNames[previewIndex]);previewImage.hidden=!previewUrl;previewError(error)}}
  finally{if(generation===previewGeneration){previewPending=false;previewRequest=null;updatePreviewButton()}}
}
window.photoPreview={close:closePreview,show(blob,name,album){return openPreview(album,name,blob)}};
$('dismiss-preview').onclick=closePreview;$('preview-previous').onclick=()=>browsePreview(-1);$('preview-next').onclick=()=>browsePreview(1);
previewDialog.addEventListener('close',()=>{if(!previewDialog.open)resetPreview()});
previewImage.onload=()=>{if(previewDialog.open&&previewImage.naturalWidth)previewStatus.hidden=true};
previewImage.onerror=()=>{if(!previewDialog.open)return;previewImage.hidden=true;previewStatus.textContent=__t('照片预览加载失败。');previewStatus.hidden=false};
$('sidebar-photo').onclick=()=>{updatePreviewButton();if($('sidebar-photo').disabled)return;openPreview(actionStatus.album.current_album,actionStatus.album.current_photo.split('/').pop())};
apply(true);})();
