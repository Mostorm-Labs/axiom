import React, {useEffect, useState} from 'react';
import {Button, Text, View} from 'react-native';
import {AxiomCanvasView, createNativeController, type NativeController} from '@mostorm/axiom-react-native';

// Mount before issuing controls. Native sessions open detached so document and
// History survive a view remount. The same Controller calls work on Web.
export default function App() {
 const [controller, setController] = useState<NativeController>();
 const [status, setStatus] = useState('Creating session');
 useEffect(() => {
  let active = true; let value:NativeController|undefined;
  void createNativeController({documentId:'0123456789abcdef0123456789abcdef',
    permission:'localSingleUser', onError:e=>setStatus(`${e.code}: ${e.message}`)})
    .then(async c=>{value=c;if(active)setController(c);else await c.close();})
    .catch(e=>setStatus(String(e)));
  return ()=>{active=false;void value?.close().catch(e=>console.error(e));};
 }, []);
 useEffect(() => controller?.subscribeState(state=>setStatus(
   `${state.objects} objects; generation ${state.generation}; undo ${state.canUndo}`)), [controller]);
 const run=(action:()=>Promise<unknown>)=>{void action().catch(e=>setStatus(String(e)));};
 return <View style={{flex:1,paddingTop:32}}>
  <Text>{status}</Text>
  <View style={{flexDirection:'row',flexWrap:'wrap'}}>
   <Button title="Round" disabled={!controller} onPress={()=>run(()=>controller!.selectBrush('round-handdrawn',4))}/>
   <Button title="Crayon" disabled={!controller} onPress={()=>run(()=>controller!.selectBrush('crayon-wax-v1',3))}/>
   <Button title="Partial eraser" disabled={!controller} onPress={()=>run(()=>controller!.selectTool('partialEraser'))}/>
   <Button title="Undo" disabled={!controller} onPress={()=>run(()=>controller!.undo())}/>
   <Button title="Redo" disabled={!controller} onPress={()=>run(()=>controller!.redo())}/>
  </View>
  {controller&&<AxiomCanvasView style={{flex:1}} sessionToken={controller.sessionToken}/>}
 </View>;
}
