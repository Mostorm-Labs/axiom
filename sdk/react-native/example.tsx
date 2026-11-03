import React, {useEffect, useState} from 'react';
import {Button, ScrollView, StyleSheet, Text, TextInput, View} from 'react-native';
import {AxiomCanvasView, createNativeController, type NativeController, DEMO_BOARD_ACTIONS, DEMO_DRAWING_TOOLS, DEMO_PRESSURE_SOURCES, DEMO_SHAPES, DEMO_SELECTION_MODES, DEMO_ERASER_MODES, resolveDemoBrushChoices, type DemoBrush} from '@mostorm/axiom-react-native';

const DOCUMENT_ID = '0123456789abcdef0123456789abcdef';
type DemoState = {objects:number;generation:string;canUndo:boolean;canRedo:boolean;activeContacts:number;camera:{scale:number};surface:{available:boolean;width:number;height:number}};

/** Android/iOS/Windows/macOS render the same control matrix as sdk/web/example.html. */
export default function App() {
 const [controller,setController]=useState<NativeController>(); const [state,setState]=useState<DemoState>();
 const [status,setStatus]=useState('正在创建会话…');
 const [pressureSource,setPressureSource]=useState<1|2>(1); const [viewMounted,setViewMounted]=useState(true);
 const [size,setSize]=useState('8'); const [opacity,setOpacity]=useState('1');
 const [brushes,setBrushes]=useState<readonly DemoBrush[]>([]);
 const selectionMode=state?.selectionMode===1?'rectangle':state?.selectionMode===2?'lasso':'point';
 const shape=state?.creationMode?(['rectangle','ellipse','triangle','line','arrow','sticky'][state.shape]??'rectangle'):'rectangle';
 useEffect(()=>{let active=true;let value:NativeController|undefined;void createNativeController({documentId:DOCUMENT_ID,permission:'localSingleUser',onError:e=>setStatus(`${e.code}: ${e.message}`)}).then(async next=>{value=next;if(active){setController(next);setStatus('就绪');}else await next.close();}).catch(e=>setStatus(String(e)));return()=>{active=false;void value?.close().catch(e=>setStatus(String(e)));};},[]);
 useEffect(()=>{if(!controller)return;const unsubscribe=controller.subscribeState(value=>setState(value as DemoState));void controller.readState().then(value=>setState(value as DemoState));return unsubscribe;},[controller]);
 useEffect(()=>{if(!controller)return;let active=true;void (async()=>{try{const available=await controller.brushPresets();const choices=resolveDemoBrushChoices(available);if(!active)return;setBrushes(choices);if(choices[0])await controller.selectBrush(choices[0].profileId,choices[0].revision);const config=await controller.readBrushConfig();if(active&&config){setPressureSource(config.pressureSource);setSize(String(config.size));setOpacity(String(config.opacity));}}catch(error){if(active)setStatus(`加载笔刷：${String(error)}`);}})();return()=>{active=false;};},[controller]);
 const run=(name:string,action:()=>Promise<unknown>)=>{void action().then(()=>setStatus(`${name} 已完成`)).catch(e=>setStatus(`${name}: ${String(e)}`));};
 const surface=state?.surface?.available?`${state.surface.width}×${state.surface.height}`:'不可用';
 const updateBrush=(source=pressureSource)=>{if(!controller)return;run('更新笔刷参数',async()=>{const config=await controller.readBrushConfig();if(config)await controller.setBrushConfig({...config,size:Number(size),opacity:Number(opacity),pressureSource:source});});};
 return <View style={styles.root}>
  <Text style={styles.title}>Axiom SDK Demo</Text><Text style={styles.status}>{status}</Text>
  <Text>对象 {state?.objects??0} · 代数 {state?.generation??'—'} · 缩放 {state?.camera?.scale?.toFixed(2)??'—'}× · Surface {surface} · Contacts {state?.activeContacts??0}</Text>
  <Text style={styles.section}>工具</Text><ScrollView horizontal contentContainerStyle={styles.toolbar}>{DEMO_DRAWING_TOOLS.map(tool=><Button key={tool.id} title={tool.label} disabled={!controller} onPress={()=>run(tool.label,()=>controller!.selectTool(tool.runtimeTool as 'ink'|'text'|'objectEraser'|'partialEraser'|'select'|'hand'))}/>)}</ScrollView>
  <Text style={styles.section}>形状</Text><ScrollView horizontal contentContainerStyle={styles.toolbar}>{DEMO_SHAPES.map(value=><Button key={value.id} title={value.label} color={shape===value.id?'#2866ba':undefined} disabled={!controller} onPress={()=>run(value.label,()=>controller!.selectShape(value.id as 'rectangle'|'ellipse'|'triangle'|'line'|'arrow'|'sticky'))}/>)}</ScrollView>
  <Text style={styles.section}>选择方式</Text><ScrollView horizontal contentContainerStyle={styles.toolbar}>{DEMO_SELECTION_MODES.map(value=><Button key={value.id} title={value.label} color={selectionMode===value.id?'#2866ba':undefined} disabled={!controller} onPress={()=>run(value.label,()=>controller!.setSelectionMode(value.id as 'point'|'rectangle'|'lasso'))}/>)}</ScrollView>
  <Text style={styles.section}>擦除方式</Text><ScrollView horizontal contentContainerStyle={styles.toolbar}>{DEMO_ERASER_MODES.map(value=><Button key={value.id} title={value.label} color={state?.eraserMode===(value.id==='objectEraser'?1:2)?'#2866ba':undefined} disabled={!controller} onPress={()=>run(value.label,()=>controller!.selectTool(value.id as 'objectEraser'|'partialEraser'))}/>)}</ScrollView>
  <Text style={styles.section}>画板</Text><ScrollView horizontal contentContainerStyle={styles.toolbar}>{DEMO_BOARD_ACTIONS.map(action=>{
   if(action.id==='undo')return <Button key={action.id} title={action.label} disabled={!controller||!state?.canUndo} onPress={()=>run(action.label,()=>controller!.undo())}/>;
   if(action.id==='redo')return <Button key={action.id} title={action.label} disabled={!controller||!state?.canRedo} onPress={()=>run(action.label,()=>controller!.redo())}/>;
   if(action.id==='fit')return <Button key={action.id} title={action.label} disabled={!controller} onPress={()=>run(action.label,()=>controller!.fitContent())}/>;
   if(action.id==='detach')return <Button key={action.id} title={action.label} disabled={!controller} onPress={()=>{setViewMounted(value=>!value);setStatus(viewMounted?'画布已分离，会话保留':'画布已重新挂载')}}/>;
   return <Button key={action.id} title={action.label} disabled={!controller} onPress={()=>setStatus(`${action.label}：不可用（由宿主应用接管）`)}/>;
  })}</ScrollView>
  <Text style={styles.section}>参数</Text><ScrollView horizontal contentContainerStyle={styles.toolbar}>{brushes.map(brush=><Button key={brush.id} title={brush.label} disabled={!controller} onPress={()=>run(`切换${brush.label}`,()=>controller!.selectBrush(brush.profileId,brush.revision))}/>)}</ScrollView>
  <View style={styles.parameterRow}><Text>压力来源</Text>{DEMO_PRESSURE_SOURCES.map(source=><Button key={source.id} title={source.label} color={pressureSource===source.id?'#2866ba':undefined} disabled={!controller} onPress={()=>{setPressureSource(source.id);updateBrush(source.id);}}/>)}</View>
  <View style={styles.parameterRow}><Text>笔宽</Text><TextInput value={size} onChangeText={setSize} keyboardType="decimal-pad" style={styles.input}/><Text>透明度</Text><TextInput value={opacity} onChangeText={setOpacity} keyboardType="decimal-pad" style={styles.input}/><Button title="应用" disabled={!controller} onPress={()=>updateBrush()}/></View>
  <View style={styles.canvasHost}>{controller&&viewMounted&&<AxiomCanvasView style={styles.canvas} sessionToken={controller.sessionToken}/>}</View>
 </View>;
}
const styles=StyleSheet.create({root:{flex:1,paddingTop:28,paddingHorizontal:12,backgroundColor:'#f5f7fa',gap:6},title:{fontSize:20,fontWeight:'600',color:'#243246'},status:{color:'#52657d'},section:{fontSize:12,color:'#7b8798',marginTop:4},toolbar:{gap:8,alignItems:'center',paddingVertical:4},parameterRow:{flexDirection:'row',alignItems:'center',gap:8},input:{width:64,height:36,borderWidth:1,borderColor:'#d7dee8',borderRadius:6,backgroundColor:'#fff',paddingHorizontal:8},canvasHost:{flex:1,minHeight:320,marginTop:4,borderWidth:1,borderColor:'#d7dee8',backgroundColor:'#fff'},canvas:{flex:1}});
