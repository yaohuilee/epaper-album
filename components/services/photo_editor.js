/* Local composition only. The ESP32 still performs six-color dithering and SD publication. */
(function(root){
'use strict';
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
const initial=()=>({turn:0,frame:null,mode:'contain',zoom:1,x:.5,y:.5});
const outputName=name=>name.replace(/\.(jpe?g|png|bmp)$/i,'.bmp');
function validName(name){return typeof name==='string'&&/\.bmp$/i.test(name)&&name.length>4&&new TextEncoder().encode(name).length<=255&&!/^[. ]|[. ]$|\.\.|[<>:"\/\\|?*\x00-\x1f\x7f]/.test(name)&&!/^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)/i.test(name);}
function suggestName(name,taken){
 const original=outputName(name);if(validName(original)&&!taken.has(original.toLowerCase()))return original;
 let base=original.slice(0,-4).replace(/[<>:"\/\\|?*\x00-\x1f\x7f]/g,'_').replace(/\.\./g,'_').replace(/^[. ]+|[. ]+$/g,'')||'photo';
 if(/^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)/i.test(base))base='photo_'+base;
 for(let n=2;;n++){const suffix='_'+n+'.bmp';let stem=base;while(new TextEncoder().encode(stem+suffix).length>255)stem=Array.from(stem).slice(0,-1).join('');const candidate=stem+suffix;if(validName(candidate)&&!taken.has(candidate.toLowerCase()))return candidate;}
}
function nameError(name,existing,recipes,index){
 if(!validName(name))return '文件名不能为空，最多255字节；不包含路径、特殊字符、连续句点或保留名称。';
 if(existing.has(name.toLowerCase()))return '相册中已有同名照片，请修改保存文件名。';
 if(recipes.some((r,i)=>i!==index&&r.name?.toLowerCase()===name.toLowerCase()))return '本批照片中有同名文件，请使用不同的保存文件名。';
 return '';
}
function surface(w,h,recipe,rotation=0){
 const frame=Number.isFinite(recipe.frame)?recipe.frame:(360-rotation)%360,portrait=frame%180!==0;
 return {frame,width:portrait?Math.min(w,h):Math.max(w,h),height:portrait?Math.max(w,h):Math.min(w,h),turn:((720-frame-rotation)/90)%4};
}
function rotate(rgba,w,h,turn){
 turn=((turn%4)+4)%4;const width=turn%2?h:w,height=turn%2?w:h;
 if(!turn)return {pixels:rgba,width,height};const pixels=new Uint8Array(rgba.length);
 for(let y=0;y<h;y++)for(let x=0;x<w;x++){
  const dx=turn===1?h-1-y:turn===2?w-1-x:y,dy=turn===1?x:turn===2?h-1-y:w-1-x;
  pixels.set(rgba.subarray((y*w+x)*4,(y*w+x+1)*4),(dy*width+dx)*4);
 }
 return {pixels,width,height};
}
function layout(sw,sh,w,h,recipe){
 const r={...initial(),...recipe},turn=((r.turn%4)+4)%4,ow=turn%2?sh:sw,oh=turn%2?sw:sh;
 const scale=(r.mode==='cover'?Math.max(w/ow,h/oh)*clamp(r.zoom,1,4):Math.min(w/ow,h/oh));
 const dw=ow*scale,dh=oh*scale;
 return {turn,scale,dw,dh,left:dw>w?(w-dw)*clamp(r.x,0,1):(w-dw)/2,top:dh>h?(h-dh)*clamp(r.y,0,1):(h-dh)/2};
}
function paint(canvas,image,recipe){
 const g=canvas.getContext('2d'),w=canvas.width,h=canvas.height,p=layout(image.naturalWidth,image.naturalHeight,w,h,recipe);
 g.fillStyle='#fff';g.fillRect(0,0,w,h);g.save();g.imageSmoothingEnabled=true;g.imageSmoothingQuality='high';
 g.translate(p.left+p.dw/2,p.top+p.dh/2);g.rotate(p.turn*Math.PI/2);g.scale(p.scale,p.scale);
 g.drawImage(image,-image.naturalWidth/2,-image.naturalHeight/2);g.restore();return p;
}
function bmp(rgba,w,h){
 const stride=(w*3+3)&~3,buffer=new ArrayBuffer(54+stride*h),v=new DataView(buffer),bytes=new Uint8Array(buffer);
 bytes[0]=66;bytes[1]=77;v.setUint32(2,buffer.byteLength,true);v.setUint32(10,54,true);v.setUint32(14,40,true);v.setInt32(18,w,true);v.setInt32(22,h,true);v.setUint16(26,1,true);v.setUint16(28,24,true);v.setUint32(34,stride*h,true);
 for(let y=0;y<h;y++)for(let x=0;x<w;x++){const s=(y*w+x)*4,d=54+(h-1-y)*stride+x*3;bytes[d]=rgba[s+2];bytes[d+1]=rgba[s+1];bytes[d+2]=rgba[s];}
 return buffer;
}
function inspect(buffer){
 const b=new Uint8Array(buffer),v=new DataView(buffer);let w=0,h=0;
 if(b.length<54)throw Error('照片数据不完整。');
 if(v.getUint32(0)===0x89504e47&&v.getUint32(4)===0x0d0a1a0a){
  if(v.getUint32(8)!==13||v.getUint32(12)!==0x49484452)throw Error('PNG 头部不合格。');w=v.getUint32(16);h=v.getUint32(20);
  let at=8,ended=false;while(at+12<=b.length){const n=v.getUint32(at),kind=v.getUint32(at+4);if(n>b.length-at-12)throw Error('PNG 数据不完整。');if([0x6163544c,0x6663544c,0x66644154].includes(kind))throw Error('不支持 APNG 动画。');at+=n+12;if(kind===0x49454e44){ended=true;break;}}if(!ended)throw Error('PNG 数据不完整。');
 }else if(v.getUint16(0)===0xffd8){
  let at=2;while(at+4<=b.length){if(b[at++]!==255)break;while(b[at]===255)at++;const marker=b[at++];if(marker===0xda||marker===0xd9)break;const n=v.getUint16(at);if(n<2||n>b.length-at)break;
   if(marker>=0xc0&&marker<=0xcf&&![0xc4,0xc8,0xcc].includes(marker)){if(marker!==0xc0||n<8||![1,3].includes(b[at+7]))throw Error('仅支持普通彩色或灰度 JPEG。');h=v.getUint16(at+3);w=v.getUint16(at+5);break;}at+=n;
  }
 }else if(v.getUint16(0,true)===0x4d42&&v.getUint16(28,true)===24&&v.getUint32(30,true)===0){w=v.getInt32(18,true);h=Math.abs(v.getInt32(22,true));}
 if(!w||!h||w<0)throw Error('图像头部不完整或格式不支持。');
 if(w>12000||h>12000||w*h>40000000)throw Error('照片宽高上限12000像素，总像素上限4000万。');
 return {width:w,height:h};
}
async function decode(file){
 inspect(await file.arrayBuffer());
 const url=URL.createObjectURL(file),image=new Image();
 try{image.src=url;await image.decode();const w=image.naturalWidth,h=image.naturalHeight;if(!w||!h||w>12000||h>12000||w*h>40000000)throw Error('照片宽高上限12000像素，总像素上限4000万。');return image;}
 catch(e){image.src='';throw Error(file.name+'：'+(e.message.includes('上限')?e.message:'浏览器无法解码此照片。'));}
 finally{URL.revokeObjectURL(url);}
}
async function prepare(file,recipe,w,h,rotation=w<h?90:0){
 const name=recipe.name||outputName(file.name);if(!validName(name))throw Error('保存文件名不合法。');
 const image=await decode(file),canvas=document.createElement('canvas'),view=surface(w,h,recipe,rotation);canvas.width=view.width;canvas.height=view.height;
 try{paint(canvas,image,recipe);const data=canvas.getContext('2d').getImageData(0,0,canvas.width,canvas.height).data,p=rotate(data,canvas.width,canvas.height,view.turn);if(p.width!==w||p.height!==h)throw Error('相框方向与设备输出尺寸不匹配。');return new File([bmp(p.pixels,w,h)],name,{type:'image/bmp'});}
 finally{image.src='';canvas.width=canvas.height=1;}
}
async function review(files,w,h,rotation=w<h?90:0,options={}){
 const $=id=>document.getElementById(id),dialog=$('compose-dialog'),canvas=$('compose-canvas'),existing=new Set((options.existingNames||[]).map(n=>n.toLowerCase())),taken=new Set(existing);
 const recipes=files.map((file,i)=>{const r={...initial(),...options.drafts?.[i]};r.name=suggestName(r.name||file.name,taken);taken.add(r.name.toLowerCase());return r;});
 let at=0,image=null,serial=0,loading=false,done=false,drag=null,settle,defaultFrame=(360-rotation)%360;
 canvas.width=w;canvas.height=h;
 function buttons(){const r=recipes[at];$('compose-back').disabled=loading||at===0;$('compose-confirm').disabled=loading||!image||!!nameError(r.name,existing,recipes,at);$('compose-confirm').textContent=at===files.length-1?'确认并上传':'下一张';for(const id of ['compose-save-name','compose-frame','compose-left','compose-right','compose-mode','compose-reset'])$(id).disabled=loading||!image;$('compose-zoom').disabled=loading||!image||r.mode!=='cover';}
 function showName(){const error=nameError(recipes[at].name,existing,recipes,at);$('compose-save-name').setAttribute('aria-invalid',String(!!error));$('compose-name-note').textContent=error||options.notice||(recipes[at].name!==outputName(files[at].name)?'将另存为新照片，原照片保留。':'文件名可用。');$('compose-name-note').classList.toggle('name-error',!!error);buttons();}
 function draw(){if(!image)return;const view=surface(w,h,recipes[at],rotation);canvas.width=view.width;canvas.height=view.height;paint(canvas,image,recipes[at]);$('compose-frame').value=String(view.frame);$('compose-mode').value=recipes[at].mode;$('compose-zoom').value=Math.round(recipes[at].zoom*100);$('compose-scale').textContent=Math.round(recipes[at].zoom*100)+'%';$('compose-info').textContent='原图 '+image.naturalWidth+' × '+image.naturalHeight+' · 构图 '+view.width+' × '+view.height+' · 存储 '+w+' × '+h;buttons();}
 async function load(){const mine=++serial;loading=true;if(image)image.src='';image=null;canvas.getContext('2d').clearRect(0,0,canvas.width,canvas.height);$('compose-name').textContent=files[at].name;$('compose-position').textContent=(at+1)+' / '+files.length;$('compose-info').textContent='正在读取本地照片…';buttons();
  try{const loaded=await decode(files[at]);if(mine!==serial||done){loaded.src='';return}image=loaded;loading=false;if(recipes[at].frame==null)recipes[at].frame=defaultFrame;$('compose-save-name').value=recipes[at].name.slice(0,-4);showName();draw();}
  catch(e){if(mine!==serial||done)return;loading=false;$('compose-info').textContent=e.message;buttons();}
 }
 function close(value){if(done)return;done=true;serial++;if(image)image.src='';image=null;canvas.width=canvas.height=1;dialog.close();settle(value);}
 const promise=new Promise(resolve=>settle=resolve);
 $('close-compose').onclick=$('compose-cancel').onclick=()=>close(null);
 $('compose-back').onclick=()=>{if(!loading&&at>0){at--;load();}};
 $('compose-confirm').onclick=()=>{if(loading||!image||nameError(recipes[at].name,existing,recipes,at))return;if(at===files.length-1){const invalid=recipes.findIndex((r,i)=>nameError(r.name,existing,recipes,i));if(invalid>=0){at=invalid;load();return}close(recipes.map(r=>({...r})));}else{at++;load();}};
 $('compose-save-name').oninput=()=>{recipes[at].name=$('compose-save-name').value+'.bmp';options.notice='';showName();};
 for(const [id,delta] of [['compose-left',3],['compose-right',1]])$(id).onclick=()=>{recipes[at].turn=(recipes[at].turn+delta)%4;recipes[at].zoom=1;recipes[at].x=recipes[at].y=.5;draw();};
 $('compose-frame').onchange=()=>{defaultFrame=Number($('compose-frame').value);recipes[at].frame=defaultFrame;recipes[at].zoom=1;recipes[at].x=recipes[at].y=.5;draw();};
 $('compose-mode').onchange=()=>{recipes[at].mode=$('compose-mode').value;recipes[at].zoom=1;recipes[at].x=recipes[at].y=.5;draw();};
 $('compose-zoom').oninput=()=>{recipes[at].zoom=Number($('compose-zoom').value)/100;draw();};
 $('compose-reset').onclick=()=>{recipes[at]={...initial(),frame:recipes[at].frame,name:recipes[at].name};draw();};
 canvas.onpointerdown=e=>{if(loading||!image||recipes[at].mode!=='cover')return;const p=layout(image.naturalWidth,image.naturalHeight,canvas.width,canvas.height,recipes[at]);drag={id:e.pointerId,px:e.clientX,py:e.clientY,x:recipes[at].x,y:recipes[at].y,p};canvas.setPointerCapture(e.pointerId);};
 canvas.onpointermove=e=>{if(!drag||drag.id!==e.pointerId)return;const rect=canvas.getBoundingClientRect(),r=recipes[at],cw=canvas.width,ch=canvas.height;if(drag.p.dw>cw)r.x=clamp(drag.x-(e.clientX-drag.px)*cw/rect.width/(drag.p.dw-cw),0,1);if(drag.p.dh>ch)r.y=clamp(drag.y-(e.clientY-drag.py)*ch/rect.height/(drag.p.dh-ch),0,1);draw();};
 canvas.onpointerup=canvas.onpointercancel=canvas.onlostpointercapture=()=>{drag=null;};
 dialog.oncancel=e=>e.preventDefault();dialog.showModal();await load();return promise;
}
const api={initial,outputName,validName,suggestName,nameError,surface,rotate,layout,paint,bmp,inspect,prepare,review};if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.photoEditor=api;
})(typeof window!=='undefined'?window:globalThis);
