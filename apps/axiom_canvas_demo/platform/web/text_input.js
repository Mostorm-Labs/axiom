// Native input carrier only. Runtime owns text, ranges, layout, history and IDs.
export function installTextInput({module,handle,canvas,stage,schedule}) {
  const events = new AbortController();
  const listen = (target, type, handler) => target.addEventListener(type, handler, {signal: events.signal});
  const carrier=document.createElement('textarea');carrier.id='axiom-ime';carrier.setAttribute('aria-label','Canvas text input');carrier.spellcheck=false;
  // The carrier follows the caret for native IME placement, but pointer selection
  // belongs to the canvas. In particular it must not steal a double-click's second Down.
  Object.assign(carrier.style,{position:'absolute',width:'1px',height:'1px',opacity:'0',pointerEvents:'none',padding:'0',border:'0',resize:'none',overflow:'hidden',whiteSpace:'pre',fontSize:'1px',lineHeight:'1px'});stage.append(carrier);
  const snapshot=()=>JSON.parse(module.UTF8ToString(module._axiom_web_text_snapshot(handle)));
  const fontCatalog=()=>JSON.parse(module.UTF8ToString(module._axiom_web_font_catalog(handle)));
  const fontStatus=id=>withString(id,value=>JSON.parse(module.UTF8ToString(module._axiom_web_font_status(handle,value))));
  const receipt=(clientId,requestId)=>withString(String(clientId),client=>
    withString(String(requestId),request=>
      JSON.parse(module.UTF8ToString(module._axiom_web_text_intent_receipt(handle,client,request)))));
  const decode=units=>{let value='';for(let i=0;i<units.length;i+=4096)value+=String.fromCharCode(...units.slice(i,i+4096));return value;};
  const withString=(value,call)=>{const ptr=module.stringToNewUTF8(value);try{return call(ptr);}finally{module._free(ptr);}};
  let keepOnBlur=false,nativeComposing=false,nativePreedit='',epoch=0n,intentSequence=0n,draining=false,disposed=false,lastEffect='',lastSession='',compositionRange=null,compositionToken=null,terminal=null,pendingTerminal=null,fault='',geometryFault='';
  const facts=[],waiters=[];
  const notify=()=>{for(const resolve of waiters.splice(0))resolve();};
  const pump=()=>{if(!module._axiom_web_text_pump(handle))throw new Error('Text owner pump rejected');};
  const wireInput=(s,fact)=>{
    const kind=fact.kind,text=(fact.text||'').replace(/\r\n?/g,'\n');const range=fact.range||(kind===0||kind===4||kind===5?s.selection:s.replacement);
    const wire={token:fact.token||s.token,sequence:s.sequence,kind,text,range,composition:kind===0?String(fact.epoch):kind===4||kind===5?'0':s.composition,effect:kind===2||kind===6?s.effect:'0',preeditSelection:fact.preeditSelection??null};
    if(fact.terminal && fact.terminal.wire)return {...fact.terminal.wire,text};
    if(fact.terminal)fact.terminal.wire=wire;
    return wire;
  };
  const send=wire=>withString(JSON.stringify(wire.token),token=>withString(wire.sequence,sequence=>withString(wire.composition,composition=>withString(wire.effect,effect=>{
    const ptr=module._malloc((wire.text.length+1)*2);try{module.stringToUTF16(wire.text,ptr,(wire.text.length+1)*2);
      const preedit=module.stringToNewUTF8(JSON.stringify(wire.preeditSelection));
      try{const queued=module._axiom_web_text_input(handle,token,sequence,wire.kind,ptr,wire.text.length,...wire.range,composition,effect,preedit);
        if(queued!==1 && queued!==2 && queued!==3)return queued;pump();return module._axiom_web_text_status(handle,token,sequence,0);
      }finally{module._free(preedit);}
    }finally{module._free(ptr);}
  }))));
  async function drain(){if(draining||disposed)return;draining=true;
    try{const accepted=[];while(facts.length){const fact=facts[0],s=snapshot();let result;
      if(fact.intent!==undefined){
        if(!fact.intentClient)fact.intentClient=String(fact.clientId??0xFC0207);
        if(!fact.intentRequest)fact.intentRequest=fact.requestId===undefined?String(++intentSequence):String(fact.requestId);
        const request={kind:fact.intent,clientId:fact.intentClient,requestId:fact.intentRequest,target:fact.target??s.target,token:fact.token??s.token,x:fact.x??0,y:fact.y??0,width:fact.width??320};
        if(fact.row!==undefined)request.row=fact.row;if(fact.column!==undefined)request.column=fact.column;if(fact.object!==undefined)request.object=fact.object;if(fact.format!==undefined)request.format=fact.format;
        result=withString(JSON.stringify(request),data=>module._axiom_web_text_intent_json(handle,data));
        if(result===1){pump();fact.receipt=receipt(fact.intentClient,fact.intentRequest);result=fact.receipt?.status??4;}
      }
      else{if(fact.kind===4)for(const [from,to] of accepted)if(JSON.stringify(fact.token)===from && JSON.stringify(s.token)===JSON.stringify(to)) {fact.token=to;if(fact.range && JSON.stringify(fact.range)===JSON.stringify(fact.selection))fact.range=s.selection;}const wire=wireInput(s,fact);result=send(wire);if(result===3 && fact.kind===4){const next=snapshot().token;for(const alias of accepted)if(JSON.stringify(alias[1])===JSON.stringify(wire.token))alias[1]=next;accepted.push([JSON.stringify(wire.token),next]);}if(result===9){setTimeout(()=>queueMicrotask(drain),0);break;}}
      facts.shift();if([4,6,7,15].includes(fact.intent) && [2,3].includes(result))fault='';if(![1,2,3].includes(result)){fault=`Text input rejected (${result}); resync or retry required`;stage.dataset.textError=fault;}
      else if(fact.intent===undefined && !fact.terminal){const wire=fact.terminal?.wire||wireInput(s,fact);withString(JSON.stringify(wire.token),token=>withString(wire.sequence,seq=>module._axiom_web_text_status(handle,token,seq,1)));}
      if(!facts.length && pendingTerminal){facts.push(pendingTerminal);pendingTerminal=null;}
    }}catch(error){fault=String(error);stage.dataset.textError=fault;}
    finally{draining=false;sync();schedule();notify();}
  }
  function enqueue(fact){if(fact.intent===undefined){const captured=snapshot();fact.token=compositionToken||captured.token;fact.selection=captured.selection;if(!fact.range && compositionRange && [0,1,2,3].includes(fact.kind))fact.range=compositionRange;}if(facts.length>=256 && fact.kind===1 && facts.at(-1)?.kind===1)facts[facts.length-1]=fact;
    else if(facts.length<257)facts.push(fact);else{if((fact.kind===2||fact.kind===3) && !pendingTerminal)pendingTerminal=fact;fault='Native input buffer full; input paused';stage.dataset.textError=fault;carrier.blur();return;}
    queueMicrotask(drain);
  }
  function sync(){if(disposed)return;const s=snapshot(),session=JSON.stringify(s.token);
    if(s.phase===0){geometryFault='';if(!fault)delete stage.dataset.textError;carrier.value='';if(document.activeElement===carrier)carrier.blur();return;}
    if(!nativeComposing && !facts.length){carrier.value=decode(s.projection);carrier.setSelectionRange(s.selection[0],s.selection[0]+s.selection[1]);}
    geometryFault=(s.geometry.diagnostics||[]).join(', ');if(fault||geometryFault)stage.dataset.textError=fault||geometryFault;else delete stage.dataset.textError;
    if(s.geometry.available){const q=s.geometry.caret;carrier.style.left=`${q[0]}px`;carrier.style.top=`${q[1]}px`;}
    if(document.activeElement===canvas && !nativeComposing)carrier.focus({preventScroll:true});
    if(s.effect!=='0' && `${s.token.session}:${s.effect}`!==lastEffect){lastEffect=`${s.token.session}:${s.effect}`;
      // Only a native compositionend may confirm the result. Never commit Update text.
      carrier.blur();if(document.activeElement===carrier){enqueue({kind:6});}
    }
    if(lastSession!==session){lastSession=session;}
  }
  async function intent(kind,options={}){enqueue({intent:kind,...options});while(draining||facts.length)await new Promise(resolve=>waiters.push(resolve));return snapshot();}
  async function complete(){keepOnBlur=true;let s=snapshot();if(s.phase===0)return true;await intent(14);sync();const until=performance.now()+800;while((s=snapshot()).composition!=="0" && s.exit!==4 && performance.now()<until)await new Promise(resolve=>setTimeout(resolve,8));if(s.composition!=="0"){fault="IME completion pending or rejected";stage.dataset.textError=fault;throw new Error(fault);}return true;}
  async function finish(){keepOnBlur=false;let s=snapshot();if(s.phase===0)return true;await intent(2);sync();
    const until=performance.now()+800;
    while((s=snapshot()).phase!==0 && s.exit!==4 && performance.now()<until)await new Promise(resolve=>setTimeout(resolve,8));
    if(s.phase!==0){fault=s.exit===4?'IME completion rejected; retry or cancel':'IME completion pending native result';stage.dataset.textError=fault;throw new Error(fault);}return true;
  }
  listen(carrier,'compositionstart',()=>{nativeComposing=true;nativePreedit='';terminal=null;compositionRange=snapshot().selection;compositionToken=snapshot().token;enqueue({kind:0,epoch:++epoch});});
  listen(carrier,'compositionupdate',event=>{nativePreedit=event.data;enqueue({kind:1,text:event.data});});
  listen(carrier,'input',event=>{
    // compositionupdate precedes the browser's carrier mutation. Read native
    // selection only after input, relative to the pinned composition range.
    if(!nativeComposing || !event.isComposing || !compositionRange)return;
    const start=carrier.selectionStart-compositionRange[0],end=carrier.selectionEnd-compositionRange[0];
    if(start<0 || end<start || end>nativePreedit.length)return;
    const update=facts.at(-1);
    if(update?.kind===1 && update.text===nativePreedit)update.preeditSelection=[start,end-start];
    else enqueue({kind:1,text:nativePreedit,preeditSelection:[start,end-start]});
  });
  listen(carrier,'compositionend',event=>{if(!nativeComposing)return;nativeComposing=false;terminal={epoch,wire:null};enqueue({kind:event.data?2:3,text:event.data,terminal});compositionRange=null;compositionToken=null;});
  listen(carrier,'beforeinput',event=>{
    if(nativeComposing||event.isComposing||['insertCompositionText','deleteCompositionText','insertFromComposition'].includes(event.inputType))return;
    event.preventDefault();if(terminal && event.inputType==='insertText'){enqueue({kind:2,text:event.data||'',terminal});return;}
    if(['insertText','insertReplacementText'].includes(event.inputType))enqueue({kind:4,text:event.data||'',range:[carrier.selectionStart,carrier.selectionEnd-carrier.selectionStart]});
    else if(event.inputType==='deleteContentBackward')enqueue({intent:8});
    else if(event.inputType==='deleteContentForward')enqueue({intent:9});
    else if(event.inputType==='insertLineBreak'||event.inputType==='insertParagraph')enqueue({intent:2});
    else{fault=`Unsupported native input: ${event.inputType}`;stage.dataset.textError=fault;sync();}
  });
  listen(carrier,'paste',event=>{if(nativeComposing)return;event.preventDefault();terminal=null;enqueue({kind:4,text:event.clipboardData.getData('text/plain')});});
  listen(carrier,'keydown',event=>{
    terminal=null;if(event.isComposing||nativeComposing||event.keyCode===229){event.stopPropagation();return;}
    const kind=event.key==='Enter'?(event.ctrlKey?3:2):event.key==='Escape'?4:event.key==='Backspace'?8:event.key==='Delete'?9:event.key==='ArrowLeft'?10:event.key==='ArrowRight'?11:(event.ctrlKey||event.metaKey)&&event.key.toLowerCase()==='a'?12:undefined;
    if(kind!==undefined){event.preventDefault();event.stopPropagation();enqueue({intent:kind});}
  });
  listen(carrier,'blur',()=>{if(snapshot().phase!==0)enqueue({intent:keepOnBlur?14:2});});
  canvas.tabIndex=0;listen(canvas,'pointerdown',()=>{keepOnBlur=false;
    // A canvas click is an editor pointer fact, not a focus exit. Moving focus
    // off the native carrier here would finish the session before Runtime can
    // locate the clicked glyph (or request native completion for an outside hit).
    if(snapshot().phase===0 || document.activeElement!==carrier)canvas.focus({preventScroll:true});
  });
  listen(canvas,'dblclick',event=>{event.preventDefault();
    // Touch double-taps are already routed through Runtime's pointer path.
    // The browser's compatibility mouse event must not submit them again.
    if(event.sourceCapabilities?.firesTouchEvents)return;
    const rect=canvas.getBoundingClientRect();finish().then(()=>{
    // Finishing the single-click draft blurs the carrier. Restore canvas focus
    // so sync can hand it to the newly entered editor rather than leaving it on body.
    canvas.focus({preventScroll:true});
    return intent(13,{x:event.clientX-rect.left,y:event.clientY-rect.top});
  }).catch(error=>{stage.dataset.textError=String(error);});});
  return {snapshot,sync,intent,finish,complete,composing:()=>nativeComposing,error:()=>fault||geometryFault,fontCatalog,fontStatus,receipt,dispose(){disposed=true;events.abort();facts.length=0;notify();carrier.remove();}};
}
