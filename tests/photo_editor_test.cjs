const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const editor=require('../components/services/photo_editor.js');
let checks=0;const near=(a,b)=>{assert(Math.abs(a-b)<1e-8);checks++};
// Entire portrait remains inside the frame, with centered white bars.
let p=editor.layout(1279,1706,800,480,editor.initial());
near(p.dh,480);near(p.top,0);near(p.left,(800-1279*480/1706)/2);
// Physical frame rotation plus firmware rotation must reconstruct the upright
// composition exactly, including asymmetric corner markers (no mirroring).
for(const rotation of [0,90,180,270])for(const frame of [0,90,180,270]){
 const w=rotation%180?4:6,h=rotation%180?6:4,view=editor.surface(w,h,{frame},rotation),rgba=new Uint8Array(view.width*view.height*4);
 for(let i=0;i<rgba.length;i++)rgba[i]=(i*17+7)%256;
 const stored=editor.rotate(rgba,view.width,view.height,view.turn);assert.equal(stored.width,w);assert.equal(stored.height,h);
 const panel=editor.rotate(stored.pixels,w,h,rotation/90),physical=editor.rotate(panel.pixels,panel.width,panel.height,frame/90);
 assert.equal(physical.width,view.width);assert.equal(physical.height,view.height);assert.deepEqual(physical.pixels,rgba);checks+=5;
}
assert.deepEqual(editor.surface(480,800,editor.initial(),90),{frame:270,width:480,height:800,turn:0});checks++;
// Fill crops vertically and the focal point reaches both edges without a gap.
for(const [x,y] of [[0,0],[.5,.5],[1,1]]){
 p=editor.layout(1279,1706,800,480,{mode:'cover',zoom:2,x,y});
 near(p.left,(800-p.dw)*x);near(p.top,(480-p.dh)*y);
}
for(const w of [480,800])for(const h of [480,800])for(const sw of [1,1279,1706,12000])for(const sh of [1,480,1706])for(let turn=0;turn<4;turn++)for(const mode of ['contain','cover']){
 p=editor.layout(sw,sh,w,h,{turn,mode,zoom:1.7,x:.2,y:.8});
 if(mode==='contain')assert(p.dw<=w+1e-8&&p.dh<=h+1e-8&&p.left>=-1e-8&&p.top>=-1e-8);
 else assert(p.dw>=w-1e-8&&p.dh>=h-1e-8&&p.left<=1e-8&&p.top<=1e-8&&p.left+p.dw>=w-1e-8&&p.top+p.dh>=h-1e-8);
 checks++;
}
// BMP is positive-height 24-bit BGR, with correct bottom-up rows and padding.
for(const [w,h] of [[1,2],[3,2],[800,480],[480,800]]){
 const rgba=new Uint8Array(w*h*4);for(let i=0;i<w*h;i++)rgba.set([i%256,(i+43)%256,(i+129)%256,255],i*4);
 const data=editor.bmp(rgba,w,h),v=new DataView(data),b=new Uint8Array(data),stride=(w*3+3)&~3;
 assert.equal(data.byteLength,54+stride*h);assert.equal(v.getInt32(18,true),w);assert.equal(v.getInt32(22,true),h);assert.equal(v.getUint16(28,true),24);
 for(const [x,y] of [[0,0],[w-1,0],[0,h-1],[w-1,h-1]]){const at=54+(h-1-y)*stride+x*3,s=(y*w+x)*4;assert.deepEqual(Array.from(b.slice(at,at+3)),[rgba[s+2],rgba[s+1],rgba[s]]);checks++;}
}
// Header checks run before allocating a browser decoded image.
const jpeg=new Uint8Array(54);jpeg.set([255,216,255,192,0,17,8,6,170,4,255,3]);
assert.deepEqual(editor.inspect(jpeg.buffer),{width:1279,height:1706});checks++;
jpeg[3]=194;assert.throws(()=>editor.inspect(jpeg.buffer),/普通/);jpeg[3]=192;jpeg[11]=4;assert.throws(()=>editor.inspect(jpeg.buffer),/普通/);checks+=2;
const png=new Uint8Array(57),pv=new DataView(png.buffer);png.set([137,80,78,71,13,10,26,10]);pv.setUint32(8,13);pv.setUint32(12,0x49484452);pv.setUint32(16,1279);pv.setUint32(20,1706);pv.setUint32(37,0x49454e44);
assert.deepEqual(editor.inspect(png.buffer),{width:1279,height:1706});pv.setUint32(37,0x6163544c);assert.throws(()=>editor.inspect(png.buffer),/APNG/);pv.setUint32(37,0x49454e44);pv.setUint32(16,12001);assert.throws(()=>editor.inspect(png.buffer),/上限/);pv.setUint32(16,1279);pv.setUint32(8,9999);assert.throws(()=>editor.inspect(png.buffer),/PNG/);checks+=4;
// UI confirmation is local; batch navigation retains recipes and cancellation resolves null.
async function ui(cancel=false,broken=false){
 const nodes=new Map();let released=0;
 const ctx2d={fillRect(){},clearRect(){},save(){},restore(){},translate(){},rotate(){},scale(){},drawImage(){}};
 function node(id){if(!nodes.has(id))nodes.set(id,{value:'',disabled:false,open:false,width:800,height:480,textContent:'',getContext:()=>ctx2d,getBoundingClientRect:()=>({width:800,height:480}),setPointerCapture(){},addEventListener(){},showModal(){this.open=true},close(){this.open=false}});return nodes.get(id);}
 class Image{constructor(){this.naturalWidth=1279;this.naturalHeight=1706}async decode(){if(broken)throw Error('bad input')}}
 const ctx={window:{},document:{getElementById:node},URL:{createObjectURL:()=> 'blob:test',revokeObjectURL:()=>released++},Image,console};vm.createContext(ctx);vm.runInContext(fs.readFileSync('components/services/photo_editor.js','utf8'),ctx);
 const fixture=editor.bmp(new Uint8Array(2*3*4),2,3),files=[{name:'portrait.bmp',arrayBuffer:async()=>fixture},{name:'next.bmp',arrayBuffer:async()=>fixture}],pending=ctx.window.photoEditor.review(files,800,480);
 for(let i=0;i<10;i++)await Promise.resolve();assert(node('compose-dialog').open);
 if(broken){assert(node('compose-confirm').disabled);assert(node('compose-info').textContent.includes('无法解码'));node('close-compose').onclick();assert.equal(await pending,null);return;}
 assert.equal(node('compose-mode').value,'contain');assert(node('compose-zoom').disabled);
 node('compose-frame').value='90';node('compose-frame').onchange();assert.equal(node('compose-canvas').width,480);assert.equal(node('compose-canvas').height,800);assert(node('compose-info').textContent.includes('构图 480 × 800'));checks+=3;
 node('compose-right').onclick();node('compose-reset').onclick();assert.equal(node('compose-frame').value,'90');assert.equal(node('compose-canvas').width,480);node('compose-mode').value='cover';node('compose-mode').onchange();node('compose-zoom').value=200;node('compose-zoom').oninput();
 node('compose-canvas').onpointerdown({pointerId:1,clientX:100,clientY:100});node('compose-canvas').onpointermove({pointerId:1,clientX:150,clientY:150});node('compose-canvas').onpointerup();
 node('compose-confirm').onclick();for(let i=0;i<10;i++)await Promise.resolve();assert.equal(node('compose-position').textContent,'2 / 2');assert.equal(node('compose-frame').value,'90');
 node('compose-right').onclick();node('compose-back').onclick();for(let i=0;i<10;i++)await Promise.resolve();assert.equal(node('compose-mode').value,'cover');assert.equal(node('compose-zoom').value,200);
 if(cancel){node('compose-cancel').onclick();assert.equal(await pending,null);}else{node('compose-confirm').onclick();for(let i=0;i<10;i++)await Promise.resolve();node('compose-confirm').onclick();const recipes=await pending;assert.equal(recipes[0].frame,90);assert.equal(recipes[1].frame,90);assert.equal(recipes[0].mode,'cover');assert.equal(recipes[0].zoom,2);assert(recipes[0].y<.5);assert.equal(recipes[1].turn,1);}
 assert(!node('compose-dialog').open);assert.equal(released,cancel?3:4);checks+=12;
}
async function prepared(){
 let rendered;
 const context={fillRect(){},save(){},restore(){},translate(){},rotate(){},scale(){},drawImage(){},getImageData(x,y,w,h){rendered=new Uint8Array(w*h*4);for(let i=0;i<rendered.length;i++)rendered[i]=(i*19+11)%256;return {data:rendered};}};
 class Image{constructor(){this.naturalWidth=2;this.naturalHeight=3}async decode(){}}
 class File{constructor(parts,name){this.buffer=parts[0];this.name=name}}
 const ctx={window:{},document:{createElement:()=>({getContext:()=>context})},URL:{createObjectURL:()=> 'blob:test',revokeObjectURL(){}},Image,File};
 vm.createContext(ctx);vm.runInContext(fs.readFileSync('components/services/photo_editor.js','utf8'),ctx);
 const fixture=editor.bmp(new Uint8Array(24),2,3),file={name:'sample.bmp',arrayBuffer:async()=>fixture};
 for(const rotation of [0,90,180,270])for(const frame of [0,90,180,270]){
  const w=rotation%180?4:6,h=rotation%180?6:4,result=await ctx.window.photoEditor.prepare(file,{frame},w,h,rotation),v=new DataView(result.buffer),bytes=new Uint8Array(result.buffer),rgba=new Uint8Array(w*h*4),stride=(w*3+3)&~3;
  assert.equal(v.getInt32(18,true),w);assert.equal(v.getInt32(22,true),h);assert.equal(result.name,'sample.bmp');
  for(let y=0;y<h;y++)for(let x=0;x<w;x++){const at=54+(h-1-y)*stride+x*3,d=(y*w+x)*4;rgba.set([bytes[at+2],bytes[at+1],bytes[at],255],d);}
  const panel=editor.rotate(rgba,w,h,rotation/90),physical=editor.rotate(panel.pixels,panel.width,panel.height,frame/90);
  for(let i=0;i<rendered.length;i+=4)assert.deepEqual(Array.from(physical.pixels.slice(i,i+3)),Array.from(rendered.slice(i,i+3)));checks+=4;
 }
}
(async()=>{await ui();await ui(true);await ui(false,true);await prepared();console.log('photo editor tests: '+checks+' geometry, BMP and batch confirmation checks passed')})().catch(e=>{console.error(e);process.exitCode=1});
