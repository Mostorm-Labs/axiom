export const SDK_VERSION = '1.0.0-preview.3';
export class AxiomError extends Error {
  constructor(code, message = code, details = {}) { super(message); this.name = 'AxiomError'; this.code = code; this.details = details; }
}
const enumValue = (values, name) => {
  if (!Object.hasOwn(values, name)) throw new AxiomError('INVALID_ARGUMENT', `Unknown value: ${name}`);
  return values[name];
};
let serial = 0;
const newId=()=>{if(!globalThis.crypto?.randomUUID)throw new AxiomError('SECURE_CONTEXT_REQUIRED','Provide an Id128 or use HTTPS/localhost');return crypto.randomUUID().replaceAll('-','');};
/** Create an isolated Runtime. Commands resolve after apply, not persistence. */
export async function createRuntime({mount, documentId, restore, initialResources, builtinFonts = true, permission = 'unconfigured',
  assetBaseURL = new URL('.', import.meta.url), onOperation, onError, onFrame, onChange} = {}) {
  if(typeof HTMLElement==='undefined')throw new AxiomError('UNSUPPORTED_ENVIRONMENT','A browser DOM and WebGL2 are required');
  documentId ??= newId();
  if (!(mount instanceof HTMLElement) || mount.children.length || !/^[0-9a-f]{32}$/.test(documentId) || /^0+$/.test(documentId))
    throw new AxiomError('INVALID_ARGUMENT', 'An empty mount element and a nonzero Id128 are required');
  const canonical = document.createElement('canvas'), preview = document.createElement('canvas');
  const id = `axiom-sdk-${++serial}-${Math.random().toString(36).slice(2)}`;
  canonical.id = `${id}-canonical`; preview.id = `${id}-preview`;
  const oldPosition = mount.style.position;
  const positioned = getComputedStyle(mount).position === 'static';
  if (positioned) mount.style.position = 'relative';
  for (const canvas of [canonical, preview]) Object.assign(canvas.style, {position:'absolute',inset:'0',width:'100%',height:'100%'});
  canonical.style.touchAction = 'none'; preview.style.pointerEvents = 'none'; preview.style.visibility = 'hidden';
  mount.append(canonical, preview);
  let module, closed = false, closing = false, scheduled = 0, observer, disposeInput, text, lastPresented = '0', tail = Promise.resolve();
  let boundWidth=0,boundHeight=0,boundDpr=0,pendingResize=false;
  let closeBarrier=false;
  const listeners=new Map();let listenerSequence=0;
  const emitError = error => { if (onError) queueMicrotask(() => {if(!closed)onError(error);}); };
  const call = (command, args = {}, bytes) => {
    if (closed) throw new AxiomError('CLOSED');
    if(bytes!==undefined && (Object.prototype.toString.call(bytes)!=='[object Uint8Array]'||bytes.length>134217728))throw new AxiomError('INVALID_ARGUMENT','Expected at most 128 MiB of bytes');
    let input = 0, data = 0;
    try {
      let serialized;
      try{serialized=JSON.stringify({command,...args});}catch(error){throw new AxiomError('INVALID_ARGUMENT',String(error));}
      input = module.stringToNewUTF8(serialized);
      if (!input) throw new AxiomError('OUT_OF_MEMORY');
      if (bytes?.length) { data = module._malloc(bytes.length); if (!data) throw new AxiomError('OUT_OF_MEMORY'); module.HEAPU8.set(bytes, data); }
      const result = JSON.parse(module.UTF8ToString(module._axiom_js_call(input, data, bytes?.length || 0)));
      if (!result.ok) throw new AxiomError(result.code || 'REJECTED', result.message || result.code, result);
      return result;
    } finally { if (input) module._free(input); if (data) module._free(data); }
  };
  const changed=()=>schedule();
  const packetValue=value=>value?{...value,owner:id}:null;
  const packetMetadata=packet=>{const {bytes,...metadata}=packet;return metadata;};
  const readPacket=after=>{
    if(after&&after.owner!==id)throw new AxiomError('WRONG_RUNTIME');
    const result=call('data',{action:'next',after:after?packetMetadata(after):null});
    return result.value?{...packetValue(result.value),bytes:module.HEAPU8.slice(module._axiom_js_bytes(),module._axiom_js_bytes()+module._axiom_js_size())}:null;
  };
  const listenerError=error=>new AxiomError('EVENT_LISTENER_FAILED',String(error));
  const dispatch=()=>{
    if(closed)return;
    for(const [token,s] of listeners){
      if(s.paused)continue;
      try{
        if(s.kind==='operation')for(let n=0;n<64&&listeners.has(token);++n){
          const packet=readPacket(s.cursor);if(!packet)break;
          const result=s.fn(packet);if(result?.then){result.catch?.(()=>{});throw new AxiomError('ASYNC_LISTENER','Transfer to your queue synchronously; report durability later');}
          if(listeners.has(token))s.cursor=packetMetadata(packet);
          if(n===63)schedule();
        }
        else if(s.kind==='state'){
          const value=call('state').value,key=JSON.stringify(value);
          if(key!==s.last){const result=s.fn(value);if(result?.then){result.catch?.(()=>{});throw new AxiomError('ASYNC_LISTENER');}s.last=key;}
        }
      }catch(error){s.paused=true;emitError(error instanceof AxiomError?error:listenerError(error));}
    }
  };
  const render = () => {
    scheduled = 0; if (closed || closing) return;
    try {
      const frame = call('frame');
      dispatch();
      if (frame.presented && frame.presented !== lastPresented) { lastPresented = frame.presented; if(onFrame)queueMicrotask(()=>{if(!closed)try{const result=onFrame({presented:frame.presented});result?.catch?.(error=>emitError(listenerError(error)));}catch(error){emitError(listenerError(error));}}); }
      if (frame.pending || frame.retry || text?.snapshot().phase) schedule();
    } catch(error) { emitError(error); }
  };
  const schedule = () => { if(!closed && !closing && !scheduled)scheduled=requestAnimationFrame(render); };
  const enqueue = (fn, finish = true) => {
    if(closing || closed || closeBarrier)return Promise.reject(new AxiomError(closeBarrier?'DOCUMENT_CLOSING':'CLOSED'));
    const task = tail.then(async()=>{if(closed)throw new AxiomError('CLOSED');if(finish)await text?.finish();const result=await fn();changed();schedule();return result;});
    tail=task.catch(()=>{});return task;
  };
  const cleanup = () => {
    listeners.clear();
    cancelAnimationFrame(scheduled); observer?.disconnect(); disposeInput?.(); text?.dispose();
    for(const [name,fn,options] of events)canonical.removeEventListener(name,fn,options);
    canonical.remove();preview.remove();if(positioned&&mount.style.position==='relative')mount.style.position=oldPosition;
  };
  const resize = () => {
    const rect=mount.getBoundingClientRect(), dpr=devicePixelRatio || 1;
    if(!(rect.width>0 && rect.height>0 && rect.width*dpr<=16384 && rect.height*dpr<=16384))throw new AxiomError('INVALID_SIZE','Mount dimensions exceed the supported surface range');
    const state=call('state').value;
    if(state.activeContacts){pendingResize=true;return;}
    if(state.surface.available && rect.width===boundWidth && rect.height===boundHeight && dpr===boundDpr)return;
    canonical.width=preview.width=Math.round(rect.width*dpr);canonical.height=preview.height=Math.round(rect.height*dpr);
    call('resize',{width:rect.width,height:rect.height,dpr});boundWidth=rect.width;boundHeight=rect.height;boundDpr=dpr;pendingResize=false;schedule();
  };
  const events = [];
  try {
    const factory = (await import(new URL('internal/axiom.mjs', import.meta.url))).default;
    const assets=new URL(assetBaseURL,import.meta.url);
    module=await factory({canvas:canonical,locateFile:name=>new URL(name,assets).href,
      onPreview:visible=>{preview.style.visibility=visible?'visible':'hidden';},
      onLocalOperation:bytes=>{if(onOperation)queueMicrotask(()=>{if(!closed)try{const result=onOperation(bytes);if(result?.catch)result.catch(error=>emitError(listenerError(error)));}catch(error){emitError(listenerError(error));}});},
      onDeliveryError:()=>emitError(new AxiomError('OPERATION_DELIVERY_FAILED'))});
    if(['_axiom_js_call','_axiom_js_bytes','_axiom_js_size','_malloc','_free','UTF8ToString','stringToNewUTF8'].some(key=>typeof module[key]!=='function'))throw new AxiomError('INCOMPATIBLE_ENGINE');
    if(call('version').apiVersion!==1||call('version').features?.shellAxis!==1)throw new AxiomError('INCOMPATIBLE_ENGINE');
    const initial=mount.getBoundingClientRect(),dpr=devicePixelRatio||1;
    if(!(initial.width>0&&initial.height>0))throw new AxiomError('INVALID_SIZE');
    canonical.width=preview.width=Math.round(initial.width*dpr);canonical.height=preview.height=Math.round(initial.height*dpr);
    call('open',{documentId,builtinFonts,width:initial.width,height:initial.height,dpr,canonical:`#${canonical.id}`,preview:`#${preview.id}`,...(restore?{restore:restore.metadata}:{}),...(initialResources?{initialResources}:{})},restore?.bytes);
    boundWidth=initial.width;boundHeight=initial.height;boundDpr=dpr;
    call('multi',{action:'permission',mode:enumValue({unconfigured:0,localSingleUser:1,contextUnavailable:2},permission)});
    resize();
    const [{installPointerInput},{installTextInput}]=await Promise.all([import('./pointer_input.mjs'),import('./text_input.mjs')]);
    text=installTextInput({module,handle:1,canvas:canonical,stage:mount,schedule});
    mount.querySelector('textarea').id=`${id}-ime`;
    disposeInput=installPointerInput({canvas:canonical,logicalSize:()=>[boundWidth,boundHeight],submit:fact=>{
      try{call('pointer',fact);text.sync();if(pendingResize&&fact.phase>=2)resize();changed();schedule();return true;}catch(error){emitError(error);return false;}
    }});
    const listen=(name,fn,options)=>{canonical.addEventListener(name,fn,options);events.push([name,fn,options]);};
    listen('dblclick',event=>{const s=call('state').value;if(s.tool!==4 && !(s.tool===1&&s.selectionMode===0))event.stopImmediatePropagation();},true);
    listen('pointermove',e=>{if(e.pointerType==='mouse'&&!e.buttons){const r=canonical.getBoundingClientRect();try{call('hover',{x:e.clientX-r.left,y:e.clientY-r.top});schedule();}catch(error){emitError(error);}}});
    listen('wheel',e=>{e.preventDefault();const r=canonical.getBoundingClientRect();try{
      if(e.ctrlKey){const s=call('state').value;call('control',{kind:'zoom',x:e.clientX-r.left,y:e.clientY-r.top,scale:s.camera.scale*Math.exp(-e.deltaY*.01)});}
      else call('control',{kind:'pan',x:-e.deltaX,y:-e.deltaY});text.sync();schedule();
    }catch(error){emitError(error);}},{passive:false});
    const contextLost=e=>{e.preventDefault();try{inputCleanup.cancelAll();call('lose');}catch(error){emitError(error);}};
    const contextRestored=()=>{if(!canonical.getContext('webgl2').isContextLost()&&!preview.getContext('webgl2').isContextLost())try{resize();}catch(error){emitError(error);}};
    for(const c of [canonical,preview]){c.addEventListener('webglcontextlost',contextLost);c.addEventListener('webglcontextrestored',contextRestored);}
    const inputCleanup=disposeInput;disposeInput=()=>{inputCleanup();for(const c of [canonical,preview]){c.removeEventListener('webglcontextlost',contextLost);c.removeEventListener('webglcontextrestored',contextRestored);}};
    observer=new ResizeObserver(()=>{if(closed||closing)return;try{resize();text.sync();}catch(error){if(error.code!=='BUSY')emitError(error);}});observer.observe(mount);
    const control=async args=>{
      const selected=['size','transform','delete','lock','visible','unlock','order'].includes(args.kind)||(args.kind==='appearance'&&args.selected!==false);
      const stamp=selected?call('state').value:null;
      return enqueue(()=>call('control',{...args,...(stamp?{expectedGeneration:stamp.generation,expectedSelectionRevision:stamp.selectionRevision}:{})}));
    };
    const section=(action,options={})=>enqueue(()=>call('section',{action,...options}));
    const textIntent=(kind,options={})=>enqueue(async()=>{const result=await text.intent(kind,options);if(text.error())throw new AxiomError('TEXT_REJECTED',text.error());return result;},false);
    const owned=(value)=>{if(!value||value.owner!==id)throw new AxiomError('WRONG_RUNTIME');return value;};
    const subscribe=(kind,fn)=>{
      if(closed||closing||closeBarrier)throw new AxiomError(closeBarrier?'DOCUMENT_CLOSING':'CLOSED');
      if(typeof fn!=='function'||listeners.size>=64)throw new AxiomError('INVALID_ARGUMENT');
      const cursor=kind==='operation'?packetValue(call('data',{action:'attach'}).value.latest):null;
      const token=++listenerSequence;listeners.set(token,{kind,fn,cursor,last:null,paused:false});schedule();
      return ()=>{listeners.delete(token);};
    };
    if(onChange)subscribe('state',onChange);
    const api={
      version:SDK_VERSION, canvas:canonical,
      readBuildInfo:()=>call('version'),
      readState:()=>call('state').value,
      readBackground:()=>call('background').value,
      setBackground:async value=>{
        if(!value||typeof value!=='object'||Object.keys(value).length!==3||
          !['plain','dots','grid'].includes(value.mode)||
          ![value.color,value.patternColor].every(c=>Array.isArray(c)&&c.length===4&&c.every(n=>typeof n==='number'&&Number.isFinite(n)&&n>=0&&n<=1))||
          value.color[3]!==1)throw new AxiomError('INVALID_ARGUMENT');
        return control({kind:'background',value:{mode:value.mode,color:[...value.color],patternColor:[...value.patternColor]}});
      },
      readSelection:()=>call('selection').value,
      subscribeOperations:fn=>subscribe('operation',fn),subscribeState:fn=>subscribe('state',fn),
      dispatchEvents:()=>Promise.resolve().then(()=>{if(closed)throw new AxiomError('CLOSED');for(const s of listeners.values())s.paused=false;dispatch();}),
      data:{attach:options=>{const s=call('data',{action:'attach',...options}).value;return {...s,latest:packetValue(s.latest)};},
        status:()=>{const s=call('data',{action:'status'}).value;return {...s,latest:packetValue(s.latest)};},
        readNext:after=>readPacket(after),
        release:packet=>{const result=call('data',{action:'release',packet:packetMetadata(owned(packet))});schedule();return result;},
        reportStatus:(packet,localDurable,cloudSynced)=>{const result=call('data',{action:'report',packet:packetMetadata(owned(packet)),localDurable,cloudSynced});schedule();return result;}},
      selectTool:async name=>{const value=enumValue({ink:0,select:1,objectEraser:2,partialEraser:2,hand:3,text:4},name);return enqueue(()=>{const result=call('control',{kind:'tool',value});if(name.endsWith('Eraser'))call('control',{kind:'eraser',mode:name==='objectEraser'?1:2});return result;});},
      selectShape:async name=>control({kind:'shape',value:enumValue({rectangle:0,ellipse:1,triangle:2,line:3,arrow:4,sticky:5},name)}),
      setSelectionMode:async name=>control({kind:'selectionMode',value:enumValue({point:0,rectangle:1,lasso:2},name)}),
      setInk:options=>control({kind:'ink',...options}),setEraser:options=>control({kind:'eraser',...options}),
      brushPresets:()=>call('presets').value,selectBrush:(profileId,revision)=>control({kind:'preset',profileId,revision}),
      setAppearance:options=>control({kind:'appearance',...options}),orderSelection:front=>control({kind:'order',front}),
      undo:()=>control({kind:'undo'}),redo:()=>control({kind:'redo'}),panBy:(x,y)=>control({kind:'pan',x,y}),
      zoomAt:(x,y,scale)=>control({kind:'zoom',x,y,scale}),fitContent:()=>control({kind:'fit'}),
      fitSelection:()=>control({kind:'fit',target:1}),fitObject:objectId=>control({kind:'fit',target:2,objectId}),
      resizeSelection:(width,height)=>control({kind:'size',width,height}),transformSelection:matrix=>control({kind:'transform',matrix}),
      deleteSelection:()=>control({kind:'delete'}),lockSelection:value=>control({kind:'lock',value}),
      setSelectionVisible:value=>control({kind:'visible',value}),unlockSelection:()=>control({kind:'unlock'}),
      createSection:options=>section(0,options),wrapInSection:options=>section(1,options),releaseSection:objectId=>section(2,{objectId}),
      readSection:objectId=>call('readSection',objectId?{objectId}:{}).value,
      selectSection:objectId=>section(6,{objectId}),selectSectionChildren:objectId=>section(7,{objectId}),
      reparent:options=>section(3,options),resizeSection:(objectId,boundary)=>section(4,{objectId,boundary}),renameSection:(objectId,title)=>section(5,{objectId,title}),
      group:()=>enqueue(()=>{const s=call('state').value;return call('section',{action:10,objects:s.selected,generation:s.generation,selectionRevision:s.selectionRevision});}),
      ungroup:objectId=>enqueue(()=>{const s=call('state').value;return call('section',{action:11,objectId:objectId??s.primary,generation:s.generation,selectionRevision:s.selectionRevision});}),
      enterGroup:objectId=>enqueue(()=>{const s=call('state').value;return call('section',{action:8,objectId:objectId??s.primary});}),leaveGroup:()=>section(9),
      text:{read:()=>{if(closed)throw new AxiomError('CLOSED');return text.snapshot();},fonts:()=>{if(closed)throw new AxiomError('CLOSED');return text.fontCatalog();},fontStatus:id=>{if(closed)throw new AxiomError('CLOSED');return text.fontStatus(id);},
        create:(x,y,width=320)=>textIntent(0,{x,y,width}),edit:object=>textIntent(1,{object}),editAtPoint:(x,y)=>textIntent(13,{x,y}),
        replace:(value,range)=>{if(typeof value!=='string'||value.length>65536)return Promise.reject(new AxiomError('INVALID_ARGUMENT'));return enqueue(()=>{const s=text.snapshot();return call('textEdit',{kind:4,token:s.token,range:range??s.selection,units:Array.from({length:value.length},(_,i)=>value.charCodeAt(i))});},false);},
        select:(start,length)=>enqueue(()=>{const s=text.snapshot();return call('textEdit',{kind:5,token:s.token,range:[start,length]});},false),
        selectAll:()=>textIntent(12),backspace:()=>textIntent(8),delete:()=>textIntent(9),moveLeft:()=>textIntent(10),moveRight:()=>textIntent(11),
        escape:()=>textIntent(4),insertParagraph:()=>textIntent(3),setWidth:width=>textIntent(5,{width}),resync:()=>textIntent(7),retry:()=>textIntent(6),
        format:patch=>enqueue(async()=>{await text.intent(15,{format:patch});if(text.error())throw new AxiomError('TEXT_REJECTED',text.error());},false),
        finish:()=>enqueue(()=>text.finish(),false),completeComposition:()=>enqueue(()=>text.complete(),false)},
      multi:{read:()=>{const p=call('multi',{action:'read'}).value;p.stamp={owner:id,token:p.stamp};return p;},
        readValues:(field,stamp,eligible)=>call('multi',{action:'values',field,stamp:owned(stamp).token,eligible}).value,
        setPermission:mode=>enqueue(()=>call('multi',{action:'permission',mode:enumValue({unconfigured:0,localSingleUser:1,contextUnavailable:2},mode)})),
        setProperty:(field,value,stamp,eligible)=>enqueue(()=>call('multi',{action:'set',field,value,stamp:owned(stamp).token,eligible})),
        setStrokeWidth:(width,stamp,eligible)=>enqueue(()=>call('multi',{action:'width',width,stamp:owned(stamp).token,eligible}))},
      resources:{list:()=>{const value=call('resource',{action:'list'}).value;for(const e of value.entries)if(e.token)e.token.owner=id;return value;},
        readBytes:token=>{call('resource',{action:'bytes',token:owned(token)});return module.HEAPU8.slice(module._axiom_js_bytes(),module._axiom_js_bytes()+module._axiom_js_size());},
        token:objectId=>{const value=call('resource',{action:'token',objectId}).value;return value?{...value,owner:id}:null;},
        receipt:objectId=>{const value=call('resource',{action:'receipt',objectId}).value;if(value)value.token.owner=id;return value;},
        publish:(token,bytes)=>enqueue(()=>call('resource',{action:'publish',token:owned(token)},bytes),false)},
      importImage:(bytes,mime)=>enqueue(()=>call('image',{action:'import',mime},bytes)),
      readSelectedImage:()=>call('image',{action:'read'}).value,readCrop:()=>call('image',{action:'readCrop'}).value,
      replaceImage:(bytes,mime)=>enqueue(()=>call('image',{action:'replace',mime},bytes)),
      resizeImage:(objectId,width,height,freeStretch=false)=>enqueue(()=>call('image',{action:'resize',objectId,width,height,freeStretch})),
      beginCrop:objectId=>enqueue(()=>call('image',{action:'crop',objectId})),
      updateCrop:options=>enqueue(()=>call('image',{action:'update',...options})),
      commitCrop:token=>enqueue(()=>call('image',{action:'done',token})),cancelCrop:token=>enqueue(()=>call('image',{action:'cancel',token})),
      createConnector:options=>enqueue(()=>call('connector',{action:'create',...options,objectId:options?.objectId??newId()})),
      setConnectorRouting:options=>enqueue(()=>call('connector',{action:'routing',...options})),
      readConnector:objectId=>call('connector',{action:'read',...(objectId?{objectId}:{})}).value,
      createSticky:options=>enqueue(()=>call('sticky',{action:'create',...options,objectId:options?.objectId??newId()})),readSelectedSticky:()=>call('sticky',{action:'read'}).value,
      captureSnapshot:metadata=>enqueue(()=>{const result=call('snapshot',metadata);return {...result,bytes:module.HEAPU8.slice(module._axiom_js_bytes(),module._axiom_js_bytes()+module._axiom_js_size())};}),
      applyRemoteOperation:bytes=>enqueue(()=>call('remote',{},bytes)),
      applyReplayOperation:bytes=>enqueue(()=>call('replay',{},bytes)),
      applyReplayOperations:packets=>enqueue(()=>call('replayBatch',{packets:packets.map(p=>({operationId:p.operationId,bytes:Array.from(p.bytes)}))}).value),
      applyRemoteOperations:packets=>enqueue(()=>call('remoteBatch',{packets:packets.map(p=>({operationId:p.operationId,bytes:Array.from(p.bytes)}))}).value),
      async close(){if(closed)return;if(closing)throw new AxiomError('BUSY');closing=true;
        try{await tail;await text.finish();call('close');disposeInput?.();disposeInput=undefined;closed=true;cleanup();text=undefined;module=null;}
        catch(error){if(['PUBLICATION_PENDING','SHUTDOWN_FAILED','CLOSE_FAILED'].includes(error.code)){closeBarrier=true;disposeInput?.();disposeInput=undefined;text.dispose();}closing=false;schedule();throw error;}}
    };
    schedule();return Object.freeze(api);
  } catch(error) {try{if(module)call('close');}catch{}closed=true;cleanup();throw error;}
}
