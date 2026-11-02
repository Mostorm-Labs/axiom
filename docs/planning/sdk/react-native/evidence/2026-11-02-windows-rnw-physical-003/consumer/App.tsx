import React, {useCallback, useEffect, useMemo, useState} from 'react';
import {Button, ScrollView, StyleSheet, Text, View} from 'react-native';
import {AxiomCanvasView, createNativeController, type NativeController} from '@mostorm/axiom-react-native';

type State = {objects:number; generation:number; canUndo:boolean; canRedo:boolean; activeContacts:number; camera:{scale:number}; surface:{available:boolean;width:number;height:number}; tool:number; eraserMode:number};
const DOCUMENT_ID = '0123456789abcdef0123456789abcdef';
const BRUSHES = [
  ['Round', 'round-handdrawn', 4], ['Marker', 'marker-compact-v1', 4],
  ['Chalk', 'chalk-compact-v1', 3], ['Crayon', 'crayon-wax-v1', 3],
  ['Watercolor', 'watercolor-lite-v1', 3], ['Laser', 'laser-transient', 1],
] as const;

export default function App() {
  const [controller, setController] = useState<NativeController>();
  const [state, setState] = useState<State>();
  const [status, setStatus] = useState('Creating native session…');
  const [viewMounted, setViewMounted] = useState(true);
  const [selectedBrush, setSelectedBrush] = useState('Round');
  const [lastCommand, setLastCommand] = useState('');

  useEffect(() => {
    let active = true;
    let value: NativeController | undefined;
    void createNativeController({
      documentId: DOCUMENT_ID,
      permission: 'localSingleUser',
      onError: error => setStatus(`${error.code}: ${error.message}`),
    }).then(async next => {
      value = next;
      if (active) { setController(next); setStatus('Ready — native D3D12 session'); }
      else await next.close();
    }).catch(error => setStatus(String(error)));
    return () => { active = false; void value?.close().catch(error => setStatus(String(error))); };
  }, []);

  useEffect(() => {
    if (!controller) return;
    const read = () => { void controller.readState().then(value => setState(value as State)).catch(error => setStatus(String(error))); };
    read();
    const unsubscribe = controller.subscribeState(value => { setState(value as State); });
    return unsubscribe;
  }, [controller]);

  const run = useCallback((name: string, action: () => Promise<unknown>) => {
    setLastCommand(name);
    void action().then(() => setStatus(`${name} OK`)).catch(error => setStatus(`${name}: ${String(error)}`));
  }, []);
  const chooseBrush = useCallback((label: string, profile: string, revision: number) => {
    if (!controller) return;
    setSelectedBrush(label);
    run(`Brush ${label}`, () => controller.selectBrush(profile, revision));
  }, [controller, run]);
  const surfaceText = state?.surface?.available ? `${state.surface.width}×${state.surface.height}` : 'Unavailable';
  const scaleText = state ? `${state.camera.scale.toFixed(2)}×` : '—';
  const controls = useMemo(() => BRUSHES.map(([label, profile, revision]) =>
    <Button key={label} title={label} disabled={!controller} onPress={() => chooseBrush(label, profile, revision)} />), [chooseBrush, controller]);

  return <View style={styles.root}>
    <Text style={styles.title}>Axiom RN Windows physical verification</Text>
    <Text testID="status">{status}</Text>
    <Text>Brush: {selectedBrush} · Objects: {state?.objects ?? 0} · Generation: {state?.generation ?? '—'} · Scale: {scaleText}</Text>
    <Text>Surface: {surfaceText} · Contacts: {state?.activeContacts ?? 0} · Last: {lastCommand || '—'}</Text>
    <ScrollView horizontal contentContainerStyle={styles.toolbar}>
      {controls}
      <Button title="Object Eraser" disabled={!controller} onPress={() => run('Object Eraser', () => controller!.selectTool('objectEraser'))} />
      <Button title="Partial Eraser" disabled={!controller} onPress={() => run('Partial Eraser', () => controller!.selectTool('partialEraser'))} />
      <Button title="Undo" disabled={!controller || !state?.canUndo} onPress={() => run('Undo', () => controller!.undo())} />
      <Button title="Redo" disabled={!controller || !state?.canRedo} onPress={() => run('Redo', () => controller!.redo())} />
      <Button title={viewMounted ? 'Detach' : 'Reattach'} disabled={!controller} onPress={() => { setViewMounted(value => !value); setStatus(viewMounted ? 'View detached; session retained' : 'View reattached'); }} />
    </ScrollView>
    <View style={styles.canvasHost}>
      {controller && viewMounted && <AxiomCanvasView style={styles.canvas} sessionToken={controller.sessionToken} />}
    </View>
  </View>;
}

const styles = StyleSheet.create({
  root: {flex: 1, padding: 18, backgroundColor: '#f7f7f7', gap: 8},
  title: {fontSize: 20, fontWeight: '600'},
  toolbar: {gap: 8, alignItems: 'center', paddingVertical: 6},
  canvasHost: {flex: 1, minHeight: 500, borderWidth: 1, borderColor: '#aaa', backgroundColor: '#fff'},
  canvas: {flex: 1},
});
