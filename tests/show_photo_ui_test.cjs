const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const script=fs.readFileSync('components/services/albums.html','utf8').match(/<script>([\s\S]*?)<\/script>/)[1].replace(/operation\(refreshAlbums\);setInterval[\s\S]*$/, '');
async function scenario(failure){
 const nodes=new Map(),calls=[],labels=[];let job=0,polls=0,tick=0,ctx;
 function node(id){if(!nodes.has(id)){let text='';const n={value:'',children:[],classList:{toggle(){},add(){},remove(){}},attrs:{},setAttribute(k,v){this.attrs[k]=v},addEventListener(){},querySelectorAll(){return []},replaceChildren(...v){this.children=v},append(...v){this.children.push(...v)},focus(){}};Object.defineProperty(n,'textContent',{get:()=>text,set:v=>{text=v;labels.push(v)}});nodes.set(id,n)}return nodes.get(id)}
 ctx={console,URL,URLSearchParams,TextEncoder,AbortSignal,Date:{now:()=>{tick+=100;return tick}},Math,setTimeout:f=>{f();return 1},clearTimeout(){},setInterval(){},location:{hash:'#photos/off'},window:{addEventListener(){},photoPreview:{close(){}}},document:{getElementById:node,createElement:()=>node('el-'+nodes.size),body:{dataset:{},append(){}}},fetch:async(path,opt={})=>{
   calls.push({path,method:opt.method||'GET',body:opt.body&&JSON.parse(opt.body)});let data;
   if(opt.method==='POST'){job=1;vm.runInContext('photoAction("show","other.bmp")',ctx);data={job_id:job}}
   else {polls++;const done=polls>=3;data={control:{token:'fixture',busy:!done,job_id:job,state:done?(failure?'failed':'succeeded'):polls===1?'queued':'running',error:failure?'Photo not displayed: ESP_FAIL':''},rtc:{},album:{current_album:done&&!failure?'off':'a',current_photo:done&&!failure?'/albums/off/纪念.bmp':'/albums/a/old.bmp',options:[]}}}
   return {ok:true,headers:{get:()=>0},json:async()=>data};
 }};
 vm.createContext(ctx);vm.runInContext(script,ctx);
 vm.runInContext('online=true;currentAlbum="off";albums=[{id:"off",directory:"停用相册",enabled:false}];listing={files:[{name:"纪念.bmp",bytes:1152054,header_valid:true}],total:1};draw()',ctx);
 let button=vm.runInContext('displayButtons[0].button',ctx);assert.equal(button.textContent,'显示此照片');assert.equal(button.disabled,false);
 await vm.runInContext('photoAction("show","纪念.bmp")',ctx);
 const posts=calls.filter(c=>c.method==='POST');assert.equal(posts.length,1);assert.equal(posts[0].path,'/api/show-photo');assert.deepEqual(posts[0].body,{schema_version:1,album:'off',name:'纪念.bmp'});
 assert(labels.includes('正在显示…'));assert.equal(ctx.document.body.dataset.operation,'');
 if(failure){assert(node('feedback-text').textContent.includes('Photo not displayed'));assert.equal(button.disabled,false);assert.equal(button.textContent,'显示此照片')}
 else {assert.equal(button.textContent,'当前照片');assert.equal(button.disabled,true);assert(node('feedback-text').textContent.includes('设备已显示 纪念.bmp'))}
}
(async()=>{await scenario(false);await scenario(true);console.log('show photo UI tests: queued completion, duplicate blocking, current marker and failure recovery passed')})().catch(e=>{console.error(e);process.exitCode=1});
