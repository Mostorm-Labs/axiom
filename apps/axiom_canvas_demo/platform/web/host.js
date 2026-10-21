const resourceUrl = name => {
  const url = new URL(name, import.meta.url);
  url.search = new URL(import.meta.url).search;
  return url.href;
};
const [{default:createModule},{installTextInput}] = await Promise.all([
  import(resourceUrl('./axiom_canvas_demo_web.js')),
  import(resourceUrl('./text_input.js')),
]);

const canvas = document.getElementById('axiom-canvas');
const preview = document.getElementById('axiom-preview');
const debug = document.getElementById('debugUi');
const toggle = document.getElementById('debugUiToggle');
const stage = document.getElementById('stage');
const status = document.getElementById('status');

export const ready = globalThis.axiomMinimalReady = (async () => {
  const module = await createModule({canvas,locateFile:resourceUrl});
  const metrics = () => {
    const rect = stage.getBoundingClientRect();
    return [Math.max(1, rect.width), Math.max(1, rect.height), devicePixelRatio || 1];
  };
  let [logicalWidth, logicalHeight, dpr] = metrics();
  const selector = module.stringToNewUTF8('#axiom-canvas');
  let handle = module._axiom_web_create(selector, logicalWidth, logicalHeight,
    Math.max(1, Math.round(logicalWidth * dpr)), Math.max(1, Math.round(logicalHeight * dpr)), dpr);
  module._free(selector);
  if (!handle) throw new Error('Minimal Web surface binding failed');
  const active = new Map(), lastTimes = new Map(), debugPointers = new Map();
  const trace = [];
  let scheduled = 0, textOpportunityTimer = 0, demand = true, pending = false, debugVisible = false, failure = '';
  const observation = () => ({...JSON.parse(module.UTF8ToString(module._axiom_web_snapshot(handle))),
    failure, trace: trace.slice(), width: canvas.width, height: canvas.height});
  const record = value => { trace.push(value); if (trace.length > 512) trace.shift(); };
  const fail = error => { failure = String(error); status.textContent = failure; console.error(error); };
  const schedule = (dirty = true) => {
    demand ||= dirty;
    if (!scheduled && handle && !document.hidden) scheduled = requestAnimationFrame(tick);
  };
  const textInput=installTextInput({module,handle,canvas,stage,schedule});
  document.getElementById("textTool").addEventListener("click",()=>textInput.finish().then(()=>{module._axiom_web_text_tool(handle);schedule();}).catch(error=>{status.textContent=String(error);}));
  const textWidth=document.getElementById('textWidth');
  textWidth.addEventListener('pointerdown',event=>{if(textInput.snapshot().phase!==0){if(textInput.snapshot().composition!=='0')event.preventDefault();textInput.complete().then(()=>textWidth.focus()).catch(error=>{stage.dataset.textError=String(error);});}});
  textWidth.addEventListener('change',()=>textInput.intent(5,{width:Number(textWidth.value)}));
  function tick() {
    scheduled = 0;
    if (!handle || document.hidden || failure || !module._axiom_web_backend_ready(handle)) return;
    try {
      if (pending) {
        const result = module._axiom_web_complete_frame(handle);
        if (!result) throw new Error(observation().error || 'Web frame completion rejected');
        if (result === 2) { schedule(false); return; }
        pending = false;
      }
      if (demand) {
        demand = false;
        if (debugVisible) module._axiom_web_debug(handle, 5, 0, 0, 0);
        const result = module._axiom_web_begin_frame(handle);
        if (!result) throw new Error(observation().error || 'Web frame submission rejected');
        pending = result === 2;
        if (result === 3) { demand = true; schedule(false); }
        if (debugVisible) module._axiom_web_debug(handle, 5, 0, 0, 0);
      }
      textInput.sync();
      const editor=textInput.snapshot();textWidth.disabled=editor.phase===0;if(document.activeElement!==textWidth)textWidth.value=editor.width;
      // Supply idle owner opportunities while editing. Runtime owns the blink
      // clock and renders only at a phase edge; this is not a repaint timer.
      if (editor.phase !== 0 && !textOpportunityTimer) {
        textOpportunityTimer = setTimeout(() => { textOpportunityTimer = 0; schedule(); }, 100);
      } else if (editor.phase === 0) {
        clearTimeout(textOpportunityTimer); textOpportunityTimer = 0;
      }
      const state = observation();
      status.textContent = (stage.dataset.textError||textInput.error()) || `对象 ${state.objects} · 操作 ${state.operations} · 缩放 ${state.camera.scale.toFixed(2)} · 帧 ${state.presentedFrames}`;
      if (pending) schedule(false);
    } catch (error) { fail(error); }
  }
  const cancelInputs = reason => {
    if (!handle) return;
    module._axiom_web_cancel_all(handle);
    const ids = [...active.values()].map(contact => contact.pointer); active.clear();
    for (const id of ids) if (canvas.hasPointerCapture?.(id)) canvas.releasePointerCapture(id);
    record({kind: 'cancel-all', reason});
    schedule();
  };
  const sourceFor = type => type === 'touch' ? 3 : type === 'pen' ? 2 : 1;
  const forward = (event, phase) => {
    if (!handle || failure) return;
    const source = sourceFor(event.pointerType);
    const identity = `${source}:${event.pointerId}`;
    let contact = active.get(identity);
    if (phase === 0) {
      if (contact) return;
      contact = {source, pointer: event.pointerId, generation: 0n};
    } else if (!contact || contact.source !== source) return;
    const history = phase === 1 && event.getCoalescedEvents ? event.getCoalescedEvents() : [];
    const samples = [...history];
    const last = samples.at(-1);
    if (!last || last.timeStamp !== event.timeStamp || last.clientX !== event.clientX || last.clientY !== event.clientY ||
        last.pressure !== event.pressure || last.width !== event.width || last.height !== event.height)
      samples.push(event);
    const rect = canvas.getBoundingClientRect();
    for (let i = 0; i < samples.length; ++i) {
      const sample = samples[i];
      let ns = BigInt(Math.max(1, Math.round(sample.timeStamp * 1000000)));
      const preceding = lastTimes.get(identity) || 0n;
      if (phase === 1 && ns <= preceding) continue;
      if (phase === 0 && ns <= preceding) { record({kind:'stale-down', source, pointer:event.pointerId}); return; }
      if (ns <= preceding) ns = preceding + 1n;
      // View coordinates only. Runtime owns view-to-content/camera mapping.
      const x = (sample.clientX - rect.left) * logicalWidth / rect.width;
      const y = (sample.clientY - rect.top) * logicalHeight / rect.height;
      const pressure = Number.isFinite(sample.pressure) ? sample.pressure : 0;
      const tool = event.pointerType === 'touch' ? 1 : event.pointerType === 'pen' ? 2 : 0;
      const geometry = tool !== 0 && sample.width > 1 && sample.height > 1;
      const accepted = module._axiom_web_pointer(handle, source, event.pointerId, contact.generation, ns,
        phase, x, y, pressure, tool === 2 && Number.isFinite(sample.pressure) ? 1 : 0,
        tool, geometry ? sample.width : 0, geometry ? sample.height : 0, geometry ? 1 : 0,
        i < history.length ? 1 : 0);
      if (!accepted) {
        active.delete(identity);
        if (canvas.hasPointerCapture?.(event.pointerId)) canvas.releasePointerCapture(event.pointerId);
        record({kind:'pointer-rejected',source,pointer:event.pointerId,generation:String(contact.generation),phase});
        status.textContent = `Pointer rejected: ${source}/${event.pointerId}/${contact.generation}, phase ${phase}`;
        schedule();
        return;
      }
      if (phase === 0) {
        contact.generation = module._axiom_web_pointer_generation(handle, source, event.pointerId);
        active.set(identity, contact);
      }
      lastTimes.set(identity, ns);
      record({kind:'pointer', source, pointer:event.pointerId, generation:String(contact.generation),
        phase, timestampNs:String(ns), x,y,pressure,history:i < history.length});
    }
    if (phase === 2 || phase === 3) {
      active.delete(identity);
      if (canvas.hasPointerCapture?.(event.pointerId)) canvas.releasePointerCapture(event.pointerId);
    }
    event.preventDefault(); schedule();
  };
  canvas.addEventListener('pointerdown', event => {
    try { canvas.setPointerCapture(event.pointerId); forward(event,0); }
    catch (error) { cancelInputs('failed-down'); record({kind:'failed-down',error:String(error)}); }
  });
  canvas.addEventListener('pointermove', event => forward(event,1));
  canvas.addEventListener('pointerup', event => forward(event,2));
  canvas.addEventListener('pointercancel', event => forward(event,3));
  canvas.addEventListener('lostpointercapture', event => {
    const identity = `${sourceFor(event.pointerType)}:${event.pointerId}`;
    const contact = active.get(identity);
    if (!contact || canvas.hasPointerCapture?.(event.pointerId)) return;
    module._axiom_web_cancel_pointer(handle, contact.source, contact.pointer, contact.generation);
    active.delete(identity); schedule();
  });
  const resize = (lw,lh,ratio, setCss = true) => {
    if (lw === logicalWidth && lh === logicalHeight && ratio === dpr && module._axiom_web_backend_ready(handle)) return;
    cancelInputs('resize');
    const text = module.stringToNewUTF8('#axiom-canvas');
    const ok = module._axiom_web_resize(handle,text,lw,lh,Math.max(1,Math.round(lw*ratio)),Math.max(1,Math.round(lh*ratio)),ratio);
    module._free(text);
    if (!ok) throw new Error('Web surface resize/rebind rejected');
    [logicalWidth,logicalHeight,dpr] = [lw,lh,ratio]; pending = false;
    failure = '';
    if (setCss) { stage.style.flex = 'none'; stage.style.width = `${lw}px`; stage.style.height = `${lh}px`; }
    schedule();
  };
  const resizeObserver = new ResizeObserver(() => {
    if (!handle) return;
    try { resize(...metrics(), false); } catch (error) { fail(error); }
  });
  resizeObserver.observe(stage);
  addEventListener('resize', () => { if (handle) resize(...metrics(), false); });
  for (const target of [canvas,preview]) {
    target.addEventListener('webglcontextlost', event => { event.preventDefault(); cancelInputs('context-loss'); module._axiom_web_lose_surface(handle); pending = false; });
    target.addEventListener('webglcontextrestored', () => { if (handle) resize(...metrics(), false); });
  }
  const flush = async () => {
    schedule();
    for (let i = 0; i < 120; ++i) {
      await new Promise(resolve => requestAnimationFrame(resolve));
      if (failure) throw new Error(failure);
      if (!scheduled && !demand && !pending) return;
    }
    throw new Error('Web frame completion timed out');
  };
  // Transport names map to the existing CanvasControlPayloadKind. Intent
  // validation and all state changes remain in the composition-owned facade.
  const controls = {tool:0,preset:1,ink:2,eraser:3,pan:4,zoom:5,setZoom:6,fit:7,undo:8,redo:9,shape:10};
  const control = async (kind,...values) => {
    if(["tool","preset","eraser","shape"].includes(kind))await textInput.finish();
    cancelInputs('facade-control');
    const args = Array.from({length:6},(_,i) => values[i] ?? 0);
    const code = typeof kind === 'string' ? controls[kind] : kind;
    if (!Number.isInteger(code) || !module._axiom_web_control(handle,code,...args)) throw new Error('RuntimeFacade control rejected');
    await flush();
    if (observation().controlState !== 1) throw new Error(`RuntimeFacade control not applied: ${observation().controlState}`);
    return observation();
  };
  const toggleDebugUi = async () => {
    try{await textInput.complete();}catch(error){status.textContent=String(error);return;}
    if (!observation().debugUiEnabled) return;
    cancelInputs('debug-toggle');
    debugVisible = !debugVisible; debug.hidden = !debugVisible;
    toggle.textContent = debugVisible ? 'Hide Debug UI' : 'Debug UI';
    module._axiom_web_debug(handle,4,0,0,0); schedule();
  };
  toggle.hidden = !observation().debugUiEnabled;
  toggle.addEventListener('click',toggleDebugUi);
  const debugPoint = event => {
    const r = debug.getBoundingClientRect();
    return [(event.clientX-r.left)*debug.width/r.width,(event.clientY-r.top)*debug.height/r.height];
  };
  debug.addEventListener("pointerdown",event=>{if(textInput.snapshot().phase!==0){const composing=textInput.snapshot().composition!=="0";if(composing){event.preventDefault();event.stopImmediatePropagation();}textInput.complete().then(()=>debug.focus()).catch(error=>{status.textContent=String(error);});}},true);
  for (const [type,phase] of [['pointerdown',0],['pointermove',1],['pointerup',2],['pointercancel',3],['lostpointercapture',3]]) {
    debug.addEventListener(type,event => {
      if (phase === 0) { cancelInputs('debug-input'); debugPointers.set(event.pointerId,event.button < 0 ? 0 : event.button); debug.setPointerCapture(event.pointerId); }
      const button = debugPointers.get(event.pointerId);
      if (button !== undefined) {
        module._axiom_web_debug(handle,phase,...debugPoint(event),button);
        if (phase === 2 || phase === 3) { debugPointers.delete(event.pointerId); if (debug.hasPointerCapture?.(event.pointerId)) debug.releasePointerCapture(event.pointerId); }
        schedule();
      } else if (phase === 1 && event.pointerType === 'mouse') {
        module._axiom_web_debug(handle,1,...debugPoint(event),0); schedule();
      }
      event.preventDefault(); event.stopPropagation();
    });
  }
  debug.addEventListener('wheel',event => { module._axiom_web_debug(handle,8,-event.deltaX/100,-event.deltaY/100,0); event.preventDefault(); event.stopPropagation(); schedule(); },{passive:false});
  canvas.addEventListener('wheel',event => {
    const r = canvas.getBoundingClientRect();
    const promise = event.ctrlKey ? control('zoom',(event.clientX-r.left)*logicalWidth/r.width,(event.clientY-r.top)*logicalHeight/r.height,Math.exp(-event.deltaY*.01))
      : control('pan',-event.deltaX,-event.deltaY);
    promise.catch(fail); event.preventDefault();
  },{passive:false});
  const keys = {Tab:512,ArrowLeft:513,ArrowRight:514,ArrowUp:515,ArrowDown:516,Backspace:523,Space:524,Enter:525,Escape:526,
    ControlLeft:527,ShiftLeft:528,AltLeft:529,ControlRight:531,ShiftRight:532,AltRight:533};
  for (const type of ['keydown','keyup']) addEventListener(type,event => {
    if (type === 'keydown' && event.code === 'F12') { event.preventDefault(); toggleDebugUi(); return; }
    if (type === 'keydown' && event.ctrlKey && (event.code === 'KeyZ' || event.code === 'KeyY')) {
      control(event.code === 'KeyZ' ? 'undo' : 'redo').catch(fail); event.preventDefault(); return;
    }
    if (!debugVisible || (document.activeElement!==debug && document.activeElement?.id==='axiom-ime')) return;
    module._axiom_web_debug(handle,9,0,0,(event.ctrlKey?1:0)|(event.shiftKey?2:0)|(event.altKey?4:0)|(event.metaKey?8:0));
    const key = keys[event.code] ?? (/^Key[A-Z]$/.test(event.code) ? 546+event.code.charCodeAt(3)-65
      : /^Digit[0-9]$/.test(event.code) ? 536+Number(event.code.slice(5)) : 0);
    if (type === 'keydown' && event.key.length === 1 && !event.ctrlKey && !event.metaKey && !event.altKey) {
      const text = module.stringToNewUTF8(event.key);
      module._axiom_web_debug_text(handle,text); module._free(text); schedule();
    }
    if (key) { module._axiom_web_debug(handle,type === 'keydown' ? 6 : 7,0,0,key); schedule(); event.preventDefault(); }
  });
  addEventListener('blur',() => { cancelInputs('focus-loss'); for (const button of debugPointers.values()) module._axiom_web_debug(handle,3,-1,-1,button); debugPointers.clear(); });
  document.addEventListener('visibilitychange',() => { if (document.hidden) { clearTimeout(textOpportunityTimer); textOpportunityTimer = 0; cancelInputs('hidden'); } else schedule(); });
  document.addEventListener('visibilitychange',()=>{if(document.hidden)textInput.finish().catch(error=>{status.textContent=String(error);});});
  addEventListener('beforeunload',event=>{if(textInput.snapshot().phase!==0){textInput.finish().catch(()=>{});event.preventDefault();event.returnValue='';}});
  addEventListener('pagehide',() => { if (handle) { clearTimeout(textOpportunityTimer); textInput.dispose(); resizeObserver.disconnect(); cancelInputs('close'); cancelAnimationFrame(scheduled); module._axiom_web_destroy(handle); handle = 0; } },{once:true});
  const host = {module,handle,observation,flush,control,resize,toggleDebugUi,textInput,
    loseSurface() { cancelInputs('surface-loss'); pending = false; return module._axiom_web_lose_surface(handle); }};
  globalThis.axiomMinimalHost = host;
  await flush();
  return host;
})();
globalThis.axiomG310Ready = globalThis.axiomMinimalReady;
globalThis.axiomMinimalReady.catch(error => { status.textContent = String(error); console.error(error); });
