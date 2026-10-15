const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const script=fs.readFileSync('components/services/albums.html','utf8').match(/<script>([\s\S]*?)<\/script>/)[1].replace(/operation\(refreshAlbums\);setInterval[\s\S]*$/, '');
async function scenario(kind){
 const nodes=new Map(),calls=[];let albums=[{id:'stable',directory:'旧相册',enabled:false}],files=kind==='delete'||kind==='partial'?[{name:'one.bmp',bytes:1152054}]:[],revision=0,job=0,state='idle',error='',checked=false;
 function node(id){if(!nodes.has(id))nodes.set(id,{id,value:'',checked:false,hidden:false,disabled:false,children:[],classList:{toggle(){},add(){},remove(){}},setAttribute(){},addEventListener(){},querySelectorAll(){return []},replaceChildren(...v){this.children=v},append(...v){this.children.push(...v)},focus(){},select(){},close(){this.open=false;this.onclose?.()},showModal(){this.open=true;if(id==='delete-album-dialog'){assert.equal(node('confirm-delete-album').disabled,files.length>0);checked=files.length>0;if(checked){node('delete-album-photos').checked=true;node('delete-album-photos').onchange();assert.equal(node('confirm-delete-album').disabled,false)}node('confirm-delete-album').onclick()}}});return nodes.get(id)}
 const status=()=>({control:{token:'fixture',busy:false,job_id:job,state,error,files_revision:revision,index_pending:false},rtc:{},album:{current_album:'',options:albums}});
 const ctx={console,URL,URLSearchParams,TextEncoder,AbortSignal,Date,Math,setTimeout:f=>{f();return 1},clearTimeout(){},setInterval(){},location:{hash:'#photos'},window:{addEventListener(){},photoPreview:{close(){}}},document:{getElementById:node,createElement:()=>node('element-'+nodes.size),body:{dataset:{},append(){}},createTextNode:s=>s},fetch:async(path,opt={})=>{calls.push({path,method:opt.method||'GET',body:opt.body&&JSON.parse(opt.body)});let data={};if(path==='/api/status')data=status();else if(path==='/api/albums')data={albums};else if(path.startsWith('/api/files'))data={files:[...files],total:files.length,next_offset:null};else if(opt.method==='DELETE'){files=[];revision++}else if(opt.method==='POST'){job++;state='succeeded';data={job_id:job};const b=JSON.parse(opt.body);if(path==='/api/album-create')albums.push({id:b.id,directory:b.directory,enabled:false});if(path==='/api/album-rename')albums[0].directory=b.directory;if(path==='/api/album-delete'){if(kind==='partial'){state='failed';error='Album directory must be empty; delete photos first'}else albums=[]}}return {ok:true,status:opt.method==='POST'?202:200,headers:{get:()=>0},json:async()=>data}}};
 vm.createContext(ctx);vm.runInContext(script,ctx);
 vm.runInContext('online=true;albums='+JSON.stringify(albums),ctx);
 assert.equal(vm.runInContext('validFolderName("旅行相册")',ctx),true);
 for(const name of ['x/y','..','CON.txt','a..b','a\\b','trailing.','x\u0000y']){ctx.invalidName=name;assert.equal(vm.runInContext('validFolderName(invalidName)',ctx),false,name)}
 if(kind==='create'||kind==='rename'){
  vm.runInContext(kind==='create'?'openFolderDialog("create")':'openFolderDialog("rename","stable")',ctx);
  node('folder-name').value='新相册';vm.runInContext('enable()',ctx);assert.equal(node('save-folder').disabled,false);
  node('folder-form').onsubmit({preventDefault(){}});
  for(let n=0;n<30;n++)await Promise.resolve();
  const command=calls.find(c=>c.path==='/api/album-'+kind);assert(command);assert.equal(command.body.directory,'新相册');if(kind==='rename')assert.equal(command.body.id,'stable');else assert.equal(command.body.enabled,false);
  assert.equal(node('folder-dialog').open,false);
 }else{
  await vm.runInContext('albumAction("delete","stable")',ctx);
  assert(checked);assert(calls.some(c=>c.method==='DELETE'&&c.path.includes('name=one.bmp')));assert(calls.some(c=>c.path==='/api/album-delete'));assert.equal(files.length,0);
  if(kind==='partial'){assert.equal(albums.length,1);assert(node('feedback-text').textContent.includes('已删除 1 张照片'));assert(node('feedback-text').textContent.includes('相册删除未完成'))}else assert.equal(albums.length,0);
 }
}
(async()=>{for(const kind of ['create','rename','delete','partial'])await scenario(kind);console.log('album UI tests: create, stable rename, confirmed delete and partial failure passed')})().catch(e=>{console.error(e);process.exitCode=1});
