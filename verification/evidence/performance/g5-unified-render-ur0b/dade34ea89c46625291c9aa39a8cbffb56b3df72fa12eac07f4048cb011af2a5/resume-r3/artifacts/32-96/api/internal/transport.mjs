import {AxiomError} from '../axiom.mjs';
import {copyValue} from './values.mjs';

// Private native transport adapter. No raw transport is returned by the factory.
export function createTransportController(port) {
  let closed=false,closing=false,tail=Promise.resolve(),textSequence=0n;
  const check=()=>{if(closed)throw new AxiomError('CLOSED');};
  const own=token=>{if(!token||token.owner!==port.owner)throw new AxiomError('WRONG_RUNTIME');return token;};
  const copy=copyValue;
  const request=async(command,args={},bytes)=>{
    check();const r=await port.request(command,args,bytes);
    if(!r?.ok)throw new AxiomError(r?.code||'REJECTED',r?.message||r?.code,r||{});
    return r;
  };
  const read=(command,args={})=>request(command,args).then(r=>r.value);
  const run=fn=>{check();if(closing)return Promise.reject(new AxiomError('BUSY'));const p=tail.then(fn);tail=p.catch(()=>{});return p;};
  const enqueue=(command,args={},bytes)=>{let a,b;try{a=copy(args);if(bytes!==undefined&&Object.prototype.toString.call(bytes)!=='[object Uint8Array]')throw new AxiomError('INVALID_ARGUMENT');b=bytes===undefined?undefined:new Uint8Array(bytes);if(b&&b.byteLength>134217728)throw new AxiomError('INVALID_ARGUMENT');}catch(e){return Promise.reject(e);}return run(()=>request(command,a,b));};
  const enumValue=(values,name)=>{if(!Object.hasOwn(values,name))throw new AxiomError('INVALID_ARGUMENT');return values[name];};
  const control=async args=>{
    const selected=['size','transform','delete','lock','visible','unlock','order'].includes(args.kind)||(args.kind==='appearance'&&args.selected!==false);
    const stamp=selected?await read('state'):null;
    return enqueue('control',{...args,...(stamp?{expectedGeneration:stamp.generation,expectedSelectionRevision:stamp.selectionRevision}:{})});
  };
  const ownedPacket=packet=>{const {bytes,owner,...rest}=own(packet);return rest;};
  const packet=r=>r?{...r,owner:port.owner}:null;
  const appearanceValue=value=>({...value,stamp:{owner:port.owner,token:value.stamp}});
  const appearance=async(action,patch,observation,token)=>{
    if(observation)own(observation.stamp);if(token)own(token);
    const r=await enqueue('appearance',{action,...(patch===undefined?{}:{patch}),...(observation?{stamp:observation.stamp.token,eligible:observation.eligible}:{}),...(token?{token:{stamp:token.stamp,serial:token.serial}}:{})});
    return r.token?{...r,token:{...r.token,owner:port.owner}}:r;
  };
  const table=async(action,options={})=>enqueue('table',{...options,action,generation:options.generation??(await read('state')).generation});
  const section=(action,options={})=>enqueue('section',{...options,action});
  const clipboard=async(action,options={},bytes)=>{
    if(!options||typeof options!=='object'||Array.isArray(options)||Object.keys(options).some(k=>k!=='offset')||
       options.offset!==undefined&&!(Array.isArray(options.offset)&&options.offset.length===2&&options.offset.every(n=>typeof n==='number'&&Number.isFinite(n))))
      throw new AxiomError('INVALID_ARGUMENT');
    if(action==='paste'&&(Object.prototype.toString.call(bytes)!=='[object Uint8Array]'||bytes.length>16777216))
      throw new AxiomError('INVALID_ARGUMENT');
    const basis=await read('multi',{action:'read'});
    return enqueue('clipboard',{...options,action,stamp:basis.stamp},bytes);
  };
  const textIntent=(kind,options={})=>run(async()=>{const s=await read('text',{action:'snapshot'});await request('text',{action:'intent',intent:{kind,clientId:'16515592',requestId:String(++textSequence),target:s.target,token:s.token,...options}});return read('text',{action:'snapshot'});});
  const api={version:port.version,
    readBuildInfo:()=>request('version'),readState:()=>read('state'),readSelection:()=>read('selection'),
    readBackground:()=>read('background'),setBackground:value=>control({kind:'background',value}),
    exportImage:options=>enqueue('exportImage',{options:options||{}}).then(r=>({...r.value,bytes:r.bytes})),
    copySelection:()=>request('clipboard',{action:'copy'}).then(r=>({...r.value,stamp:{owner:port.owner,token:copy(r.value.stamp)},bytes:new Uint8Array(r.bytes)})),
    cutSelection:stamp=>enqueue('clipboard',{action:'cut',stamp:own(stamp).token}),
    duplicateSelection:options=>clipboard('duplicate',options),pasteObjects:(bytes,options)=>clipboard('paste',options,bytes),
    subscribeState:fn=>{check();return port.subscribe('state',fn);},subscribeOperations:fn=>{check();return port.subscribe('operation',fn);},dispatchEvents:()=>port.dispatchEvents?.()||Promise.resolve(),
    selectTool:async name=>{const value=enumValue({ink:0,select:1,objectEraser:2,partialEraser:2,hand:3,text:4},name);return run(async()=>{const r=await request('control',{kind:'tool',value});if(name.endsWith('Eraser'))await request('control',{kind:'eraser',mode:name==='objectEraser'?1:2});return r;});},
    selectShape:async name=>control({kind:'shape',value:enumValue({rectangle:0,ellipse:1,triangle:2,line:3,arrow:4,sticky:5,table:6,section:7},name)}),
    setSelectionMode:async name=>control({kind:'selectionMode',value:enumValue({point:0,rectangle:1,lasso:2},name)}),
    setInk:options=>control({kind:'ink',...options}),setEraser:options=>control({kind:'eraser',...options}),
    brushPresets:()=>read('presets'),selectBrush:(profileId,revision)=>control({kind:'preset',profileId,revision}),
    setBrushConfig:value=>control({kind:'inkBrush',value}),readBrushConfig:()=>read('readInkBrush'),
    setAppearance:options=>control({kind:'appearance',...options}),orderSelection:front=>control({kind:'order',front}),
    undo:()=>control({kind:'undo'}),redo:()=>control({kind:'redo'}),panBy:(x,y)=>control({kind:'pan',x,y}),zoomAt:(x,y,scale)=>control({kind:'zoom',x,y,scale}),
    fitContent:()=>control({kind:'fit'}),fitSelection:()=>control({kind:'fit',target:1}),fitObject:objectId=>control({kind:'fit',target:2,objectId}),
    resizeSelection:(width,height)=>control({kind:'size',width,height}),transformSelection:matrix=>control({kind:'transform',matrix}),
    deleteSelection:()=>control({kind:'delete'}),lockSelection:value=>control({kind:'lock',value}),setSelectionVisible:value=>control({kind:'visible',value}),unlockSelection:()=>control({kind:'unlock'}),
    createSection:options=>section(0,options),wrapInSection:options=>section(1,options),releaseSection:objectId=>section(2,{objectId}),
    reparent:options=>section(3,options),resizeSection:(objectId,boundary)=>section(4,{objectId,boundary}),renameSection:(objectId,title)=>section(5,{objectId,title}),
    selectSection:objectId=>section(6,{objectId}),selectSectionChildren:objectId=>section(7,{objectId}),readSection:objectId=>read('readSection',objectId?{objectId}:{}),
    group:()=>run(async()=>{const s=await read('state');return request('section',{action:10,objects:s.selected,generation:s.generation,selectionRevision:s.selectionRevision});}),
    ungroup:objectId=>run(async()=>{const s=await read('state');return request('section',{action:11,objectId:objectId??s.primary,generation:s.generation,selectionRevision:s.selectionRevision});}),
    enterGroup:objectId=>run(async()=>{const s=await read('state');return request('section',{action:8,objectId:objectId??s.primary});}),leaveGroup:()=>section(9),
    createVectorPath:options=>enqueue('vectorPath',{action:'create',...options}),readVectorPath:objectId=>read('vectorPath',{action:'read',...(objectId?{objectId}:{})}),
    createConnector:options=>enqueue('connector',{action:'create',...options}),setConnectorRouting:options=>enqueue('connector',{action:'routing',...options}),readConnector:objectId=>read('connector',{action:'read',...(objectId?{objectId}:{})}),
    createSticky:options=>enqueue('sticky',{action:'create',...options}),readSelectedSticky:()=>read('sticky',{action:'read'}),
    importImage:(bytes,mime)=>enqueue('image',{action:'import',mime},bytes),replaceImage:(bytes,mime)=>enqueue('image',{action:'replace',mime},bytes),readSelectedImage:()=>read('image',{action:'read'}),readCrop:()=>read('image',{action:'readCrop'}),
    resizeImage:(objectId,width,height,freeStretch=false)=>enqueue('image',{action:'resize',objectId,width,height,freeStretch}),beginCrop:objectId=>enqueue('image',{action:'crop',objectId}),updateCrop:options=>enqueue('image',{action:'update',...options}),commitCrop:token=>enqueue('image',{action:'done',token}),cancelCrop:token=>enqueue('image',{action:'cancel',token}),
    captureSnapshot:metadata=>enqueue('snapshot',metadata),applyRemoteOperation:bytes=>enqueue('remote',{},bytes),applyReplayOperation:bytes=>enqueue('replay',{},bytes),
    applyRemoteOperations:packets=>enqueue('remoteBatch',{packets:packets.map(p=>({operationId:p.operationId,bytes:Array.from(p.bytes)}))}).then(r=>r.value),
    applyReplayOperations:packets=>enqueue('replayBatch',{packets:packets.map(p=>({operationId:p.operationId,bytes:Array.from(p.bytes)}))}).then(r=>r.value),
    data:{attach:options=>enqueue('data',{action:'attach',...options}).then(r=>({...r.value,latest:packet(r.value.latest)})),status:()=>read('data',{action:'status'}).then(s=>({...s,latest:packet(s.latest)})),
      readNext:async after=>{const r=await request('data',{action:'next',after:after?ownedPacket(after):null});return r.value?{...packet(r.value),bytes:r.bytes}:null;},
      release:async through=>enqueue('data',{action:'release',packet:ownedPacket(through)}),reportStatus:async(p,localDurable,cloudSynced)=>enqueue('data',{action:'report',packet:ownedPacket(p),localDurable,cloudSynced})},
    appearance:{read:()=>read('appearance',{action:'read'}).then(appearanceValue),readDefaults:()=>read('appearance',{action:'readDefaults'}).then(appearanceValue),
      patch:async(p,o)=>appearance('patch',p,o??await api.appearance.read()),setDefaults:p=>appearance('defaults',p),begin:async o=>appearance('begin',undefined,o??await api.appearance.read()),
      updatePreview:(t,p)=>appearance('preview',p,undefined,t),commit:t=>appearance('commit',undefined,undefined,t),cancel:t=>appearance('cancel',undefined,undefined,t)},
    table:{create:o=>table('create',o),read:objectId=>read('table',{action:'read',...(objectId?{objectId}:{})}),select:o=>table('select',o),edit:o=>table('select',o).then(r=>textIntent(1,{object:r.objectId,row:o.rowId,column:o.columnId})),
      navigate:async d=>table(enumValue({'1':'next','-1':'previous'},String(d))),insertRow:o=>table('insertRow',o),insertColumn:o=>table('insertColumn',o),removeRow:o=>table('removeRow',o),removeColumn:o=>table('removeColumn',o),setRowHeight:o=>table('rowHeight',o),setColumnWidth:o=>table('columnWidth',o),setCellStyle:o=>table('cellStyle',o),setGridStyle:o=>table('gridStyle',o)},
    multi:{read:()=>read('multi',{action:'read'}).then(p=>({...p,stamp:{owner:port.owner,token:p.stamp}})),
      readValues:async(field,stamp,eligible)=>read('multi',{action:'values',field,stamp:own(stamp).token,eligible}),setPermission:async mode=>enqueue('multi',{action:'permission',mode:enumValue({unconfigured:0,localSingleUser:1,contextUnavailable:2},mode)}),
      setProperty:async(field,value,stamp,eligible)=>enqueue('multi',{action:'set',field,value,stamp:own(stamp).token,eligible}),setStrokeWidth:async(width,stamp,eligible)=>enqueue('multi',{action:'width',width,stamp:own(stamp).token,eligible})},
    resources:{list:()=>read('resource',{action:'list'}).then(v=>({...v,entries:v.entries.map(e=>({...e,token:packet(e.token)}))})),token:objectId=>read('resource',{action:'token',objectId}).then(packet),
      receipt:objectId=>read('resource',{action:'receipt',objectId}).then(v=>v?{...v,token:packet(v.token)}:null),
      readBytes:async token=>(await request('resource',{action:'bytes',token:own(token)})).bytes,
      publish:async(token,bytes)=>enqueue('resource',{action:'publish',token:own(token)},bytes)},
    text:{read:()=>read('text',{action:'snapshot'}),fonts:()=>read('text',{action:'fonts'}),fontStatus:id=>read('text',{action:'fontStatus',id}),
      create:(x,y,width=320)=>textIntent(0,{x,y,width}),edit:object=>textIntent(1,{object}),editAtPoint:(x,y)=>textIntent(13,{x,y}),
      replace:(value,range)=>run(async()=>{if(typeof value!=='string'||value.length>65536)throw new AxiomError('INVALID_ARGUMENT');const s=await read('text',{action:'snapshot'});return request('textEdit',{kind:4,token:s.token,range:range??s.selection,units:Array.from({length:value.length},(_,i)=>value.charCodeAt(i))});}),
      select:(start,length)=>run(async()=>{const s=await read('text',{action:'snapshot'});return request('textEdit',{kind:5,token:s.token,range:[start,length]});}),
      selectAll:()=>textIntent(12),backspace:()=>textIntent(8),delete:()=>textIntent(9),moveLeft:()=>textIntent(10),moveRight:()=>textIntent(11),escape:()=>textIntent(4),insertParagraph:()=>textIntent(3),setWidth:width=>textIntent(5,{width}),resync:()=>textIntent(7),retry:()=>textIntent(6),
      format:patch=>textIntent(15,{format:patch}).then(()=>{}),finish:()=>textIntent(2).then(s=>s.phase===0),completeComposition:()=>textIntent(14).then(s=>s.composition==='0')},
    async close(){if(closed)return;if(closing)throw new AxiomError('BUSY');closing=true;try{await tail;await port.close();closed=true;}finally{closing=false;}}
  };
  // Every argument is copied immediately, including values awaited by compound commands.
  const wrap=object=>Object.freeze(Object.fromEntries(Object.entries(object).map(([key,value])=>[key,
    typeof value==='function'&&key!=='close'&&!key.startsWith('subscribe')?async(...args)=>{check();if(closing)throw new AxiomError('BUSY');return value(...copy(args));}:
    value&&typeof value==='object'?wrap(value):value])));
  return wrap(api);
}
