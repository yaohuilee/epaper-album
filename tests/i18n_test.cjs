const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const source=fs.readFileSync('components/services/i18n.js','utf8');
const catalog=JSON.parse(source.match(/const en=(\{[\s\S]*?\});\r?\nconst supported=/)[1]);
let checks=0;
function boot({url='http://device/',saved=null,browser='en-US',broken=false,operation='',dialog=false}={}){
 const attrs={title:'预览当前照片','aria-label':'保存文件名',placeholder:'输入网络密码',alt:'照片预览'},text={nodeValue:'  照片库  ',parentElement:{closest:()=>null}},code={nodeValue:'照片库',parentElement:{closest:()=>({})}};
 const nav={href:'/albums#photos',getAttribute(){return this.href},setAttribute(k,v){this[k]=v}},host={children:[],append(n){this.children.push(n)}};let index=-1,observer;
 const context={URL,Intl,Date,performance:{now:()=>0},setInterval(){},NodeFilter:{SHOW_TEXT:4},navigator:{language:browser},localStorage:{getItem(){if(broken)throw Error('blocked');return saved},setItem(k,v){if(broken)throw Error('blocked');assert.equal(k,'epaper.language');saved=v}},location:{href:url,replace(v){this.href=v}},window:{},MutationObserver:class{constructor(f){observer=f}observe(){}},document:{documentElement:{lang:''},body:{dataset:{operation}},createTreeWalker(){return {nextNode(){index++;this.currentNode=[text,code][index];return index<2}}},querySelector(s){return s==='.header-right'?host:s==='dialog[open]'&&dialog?{}:null},querySelectorAll(selector){return selector==='nav a'?[nav]:[{getAttribute:k=>attrs[k],setAttribute:(k,v)=>attrs[k]=v}]},createElement(tag){return {tag,children:[],append(n){this.children.push(n)},setAttribute(k,v){this[k]=v}}}}};
 vm.runInNewContext(source,context);return {context,attrs,text,code,host,nav,get saved(){return saved},observe:()=>observer()};
}
for(const [options,lang]of [[{},'en'],[{browser:'zh-TW'},'zh-CN'],[{browser:'fr'},'en'],[{saved:'zh-CN'},'zh-CN'],[{saved:'bad'},'en'],[{saved:'en',url:'http://device/?lang=zh-CN#network'},'zh-CN'],[{saved:'zh-CN',url:'http://device/?lang=xx'},'zh-CN'],[{broken:true,browser:'en'},'en'],[{broken:true,url:'http://device/?lang=zh-CN'},'zh-CN']]){
 const b=boot(options);assert.equal(b.context.document.documentElement.lang,lang);assert.equal(b.context.window.epaperI18n.language,lang);assert.equal(b.text.nodeValue,lang==='en'?'  Photo library  ':'  照片库  ');assert.equal(b.code.nodeValue,'照片库');assert.equal(b.attrs.title,lang==='en'?'Preview current photo':'预览当前照片');assert.equal(b.context.window.epaperI18n.t('王颗 photo.bmp'),'王颗 photo.bmp');checks+=6;
}
const b=boot({url:'http://device/albums#photos/user-album'}),select=b.host.children[0].children[0];select.value='zh-CN';select.onchange();assert.equal(b.nav.href,'/albums?lang=en#photos');assert.equal(b.saved,'zh-CN');assert.equal(b.context.location.href,'http://device/albums?lang=zh-CN#photos/user-album');checks+=3;
for(const opts of [{operation:'busy'},{dialog:true}]){const c=boot(opts),s=c.host.children[0].children[0];assert(s.disabled);s.value='zh-CN';s.onchange();assert.equal(c.context.location.href,'http://device/');checks+=2;}
for(const f of ['ui.js','announcements.js','photo_editor.js','status.html','albums.html','setup.html']){
 const s=fs.readFileSync('components/services/'+f,'utf8');
 for(const m of s.matchAll(/__t\(('(?:\\.|[^'\\])*'|"(?:\\.|[^"\\])*")\)/g)){const key=vm.runInNewContext(m[1]);assert(Object.hasOwn(catalog,key),'Missing catalog key: '+key);checks++;}
 if(f.endsWith('.html')){assert(s.indexOf('/i18n.js')<s.indexOf('<script>'));const html=s.replace(/<script[\s\S]*?<\/script>/g,'').replace(/<style[\s\S]*?<\/style>/g,'');for(const m of html.matchAll(/>([^<>]*[\u3400-\u9fff][^<>]*)</g)){assert(Object.hasOwn(catalog,m[1].trim()),'Missing static translation: '+m[1]);checks++;}}
}
const ui=fs.readFileSync('components/services/ui.js','utf8').split('const clockNode=')[0]+'})();';
for(const lang of ['en','zh-CN']){const ctx={window:{epaperI18n:{language:lang,t:s=>lang==='en'?catalog[s]??s:s}},document:{body:{dataset:{}}},Intl,Date};vm.runInNewContext(ui,ctx);assert.equal(ctx.window.formatDuration(900),lang==='en'?'15 min':'15分钟');assert.equal(ctx.window.formatDuration(3900),lang==='en'?'1 h 5 min':'1小时5分钟');assert.equal(ctx.window.formatDuration(0),lang==='en'?'0 s':'0秒');assert.equal(ctx.window.formatDuration(null),'—');assert.equal(ctx.window.dateTime('2026-10-18T00:00:00Z'),'2026-10-18 08:00:00');checks+=5;}
// Setup has no modal: pending Wi-Fi saves and scans must also lock locale reload.
const setupNodes=new Map(),setupContext={window:{epaperI18n:{t:s=>catalog[s]??s}},document:{body:{dataset:{}},getElementById(id){if(!setupNodes.has(id))setupNodes.set(id,{value:'',checked:false,querySelectorAll:()=>[]});return setupNodes.get(id)}}};
const setupScript=fs.readFileSync('components/services/setup.html','utf8').match(/<script>([\s\S]*?)<\/script>/)[1].replace('pollTimer=setInterval(refresh,2000);refresh();','');
vm.createContext(setupContext);vm.runInContext(setupScript,setupContext);
for(const assignment of ['sending=true','sending=false;scanning=true','scanning=false;job=1','job=null']){vm.runInContext(assignment+';controls()',setupContext);assert.equal(setupContext.document.body.dataset.operation,assignment==='job=null'?'':'busy');checks++;}
// Exercise the existing real upload handler in English, including conflicts,
// busy responses, unknown transfers, partial completion and Chinese save names.
const pairs=Object.entries(catalog).sort((a,b)=>b[0].length-a[0].length);
function translatedFixture(s){if(Object.hasOwn(catalog,s))return catalog[s];if(s==='等待设备任务')return 'Waiting for device task';if(s==='已取消')return 'canceled';if(s==='上传响应未确认')return 'Upload response not confirmed';if(s==='SD卡中的目标位置无法访问')return 'The SD card destination is inaccessible';let result=s;for(const [key,value]of pairs)result=result.split(key).join(value);return result;}
let regression=fs.readFileSync('tests/import_ui_test.cjs','utf8');
regression=regression.replace('window:{addEventListener','window:{epaperI18n:{t:s=>catalog[s]??s},addEventListener');
regression=regression.replace(/\.includes\(('(?:\\.|[^'\\])*')\)/g,(all,literal)=>/[\u3400-\u9fff]/.test(literal)?'.includes(translatedFixture('+literal+'))':all);
regression=regression.replace(".includes(mode==='race-cancel'?'已取消':'本批已完成 1 / 2')",".includes(translatedFixture(mode==='race-cancel'?'已取消':'本批已完成 1 / 2'))").replace(".includes(mode==='busy-always'?'设备忙碌':'上传响应未确认')",".includes(translatedFixture(mode==='busy-always'?'设备忙碌':'上传响应未确认'))");
const sandbox={require:require('node:module').createRequire(path.resolve('tests/import_ui_test.cjs')),console,URL,URLSearchParams,TextEncoder,AbortSignal,DataView,Date,Math,process,catalog,translatedFixture};
vm.runInNewContext(regression,sandbox,{filename:'english-import-regression.cjs'});
console.log('i18n tests:',checks,'locale, catalog, accessibility and duration checks passed; English import regression started');
