import React, {useEffect, useRef, useState} from 'react';
import {AppState, Button, ScrollView, Text, View} from 'react-native';
import {SafeAreaProvider, SafeAreaView} from 'react-native-safe-area-context';
import {AxiomCanvasView, createNativeController, type NativeController, type RuntimeState} from '@mostorm/axiom-react-native';

// Physical-test Shell: only public Controller + View, no geometry/state owner.
const brushes = [
 ['Round','round-handdrawn',4], ['Marker','marker-compact-v1',4],
 ['Chalk','chalk-compact-v1',3], ['Crayon','crayon-wax-v1',3],
 ['Watercolor','watercolor-lite-v1',3], ['Laser','laser-transient',1],
] as const;
function TestCanvas() {
 const [controller,setController]=useState<NativeController>();
 const [state,setState]=useState<RuntimeState>();
 const [choice,setChoice]=useState('Round');
 const [error,setError]=useState('');
 const [mounted,setMounted]=useState(true);
 const [appState,setAppState]=useState(AppState.currentState);
 const initialized=useRef(false);
 useEffect(()=>{
  let active=true;let value:NativeController|undefined;
  void createNativeController({documentId:'0123456789abcdef0123456789abcdef',permission:'localSingleUser',
   onError:e=>setError(`${e.code}: ${e.message}`)})
   .then(async c=>{value=c;if(active){setController(c);}else await c.close();})
   .catch(e=>setError(String(e)));
  const listener=AppState.addEventListener('change',s=>{setAppState(s);console.info('[axiom-physical-lifecycle]',s);});
  return()=>{active=false;listener.remove();void value?.close().catch(e=>console.error(e));};
 },[]);
 useEffect(()=>controller?.subscribeState(s=>{
  setState(s);console.info('[axiom-physical-state]',JSON.stringify(s));
  if(!initialized.current){initialized.current=true;void controller.readBuildInfo().then(info=>{console.info('[axiom-physical-build]',JSON.stringify(info));return controller.selectTool('ink');})
   .then(()=>controller.selectBrush('round-handdrawn',4)).catch(e=>setError(String(e)));}
 }),[controller]);
 const run=(fn:()=>Promise<unknown>)=>{setError('');void fn().then(r=>console.info('[axiom-physical-receipt]',JSON.stringify(r))).catch(e=>{setError(String(e));console.error('[axiom-physical-error]',String(e));});};
 const disabled=!controller||!state||!mounted;
 return <SafeAreaView style={{flex:1,backgroundColor:'white'}}>
  <Text style={{fontSize:15,paddingHorizontal:8}}>Axiom RN 0.2.0 · iPhone physical test</Text>
  <Text style={{paddingHorizontal:8}}>{state?`${state.objects} objects · gen ${state.generation} · contacts ${state.activeContacts} · zoom ${state.camera.scale.toFixed(2)}`:'Attaching Runtime…'}</Text>
  <Text style={{paddingHorizontal:8}}>{choice} · {appState} · Undo {String(state?.canUndo??false)} · Redo {String(state?.canRedo??false)}</Text>
  <ScrollView horizontal style={{flexGrow:0,maxHeight:48}} contentContainerStyle={{alignItems:'center'}}>
   {brushes.map(([label,id,revision])=><Button key={id} title={label} disabled={disabled} onPress={()=>run(async()=>{await controller!.selectTool('ink');await controller!.selectBrush(id,revision);setChoice(label);})}/>)}
  </ScrollView>
  <View style={{flexDirection:'row',flexWrap:'wrap'}}>
   <Button title="Object erase" disabled={disabled} onPress={()=>run(async()=>{await controller!.selectTool('objectEraser');setChoice('Object erase');})}/>
   <Button title="Partial erase" disabled={disabled} onPress={()=>run(async()=>{await controller!.selectTool('partialEraser');setChoice('Partial erase');})}/>
   <Button title="Undo" disabled={disabled||!state?.canUndo} onPress={()=>run(()=>controller!.undo())}/>
   <Button title="Redo" disabled={disabled||!state?.canRedo} onPress={()=>run(()=>controller!.redo())}/>
   <Button title={mounted?'Detach':'Reattach'} disabled={!controller} onPress={()=>setMounted(v=>!v)}/>
  </View>
  {!!error&&<Text selectable style={{color:'#b00020',padding:8}}>{error}</Text>}
  <View style={{flex:1,borderTopWidth:1,borderTopColor:'#ccc'}}>
   {controller&&mounted&&<AxiomCanvasView style={{flex:1}} sessionToken={controller.sessionToken}/>}
   {!mounted&&<Text>Surface detached. Tap Reattach; document and Undo should remain.</Text>}
  </View>
 </SafeAreaView>;
}
export default function App(){return <SafeAreaProvider><TestCanvas/></SafeAreaProvider>;}
