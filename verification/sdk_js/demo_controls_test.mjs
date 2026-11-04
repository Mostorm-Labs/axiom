import assert from 'node:assert/strict';
import test from 'node:test';

import {
  DEMO_BOARD_ACTIONS,
  DEMO_BRUSHES,
  DEMO_DRAWING_TOOLS,
  DEMO_SHAPES,
  DEMO_SELECTION_MODES,
  DEMO_ERASER_MODES,
  resolveDemoBrushChoices,
} from '../../sdk/api/demo-controls.mjs';

test('shared demo control model exposes the same six brushes and tool actions', () => {
  assert.deepEqual(DEMO_BRUSHES.map(({profileId}) => profileId), [
    'round-handdrawn', 'marker-compact-v1', 'chalk-compact-v1',
    'crayon-wax-v1', 'watercolor-lite-v1', 'laser-transient',
  ]);
  assert.deepEqual(DEMO_DRAWING_TOOLS.map(({id}) => id), [
    'ink', 'text', 'objectEraser', 'partialEraser', 'select', 'hand',
  ]);
  assert.deepEqual(DEMO_BOARD_ACTIONS.map(({id}) => id), [
    'undo', 'redo', 'fit', 'background', 'export', 'detach',
  ]);
  assert.deepEqual(DEMO_SHAPES.map(({id}) => id), ['rectangle', 'ellipse', 'triangle', 'line', 'arrow', 'sticky']);
  assert.deepEqual(DEMO_SELECTION_MODES.map(({id}) => id), ['point', 'rectangle', 'lasso']);
  assert.deepEqual(DEMO_ERASER_MODES.map(({id}) => id), ['objectEraser', 'partialEraser']);
});

test('brush choices select the highest available package revision deterministically', () => {
  const choices = resolveDemoBrushChoices([
    {profileId: 'crayon-wax-v1', revision: 2},
    {profileId: 'round-handdrawn', revision: 3},
    {profileId: 'round-handdrawn', revision: 4},
    {profileId: 'laser-transient', revision: 1},
  ]);
  assert.deepEqual(choices.map(({profileId, revision}) => [profileId, revision]), [
    ['round-handdrawn', 4], ['crayon-wax-v1', 2], ['laser-transient', 1],
  ]);
});

test('React Native demo consumes the shared control model and reads native brush state asynchronously', async () => {
  const source = await (await import('node:fs/promises')).readFile(new URL('../../sdk/react-native/example.tsx', import.meta.url), 'utf8');
  for (const token of [
    'DEMO_BOARD_ACTIONS', 'DEMO_DRAWING_TOOLS', 'DEMO_PRESSURE_SOURCES', 'DEMO_SHAPES', 'DEMO_SELECTION_MODES', 'DEMO_ERASER_MODES', 'resolveDemoBrushChoices',
    'await controller.brushPresets()', 'await controller.readBrushConfig()', 'state?.selectionMode', 'state?.eraserMode',
  ]) assert.match(source, new RegExp(token.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')));
});

test('Web demo consumes the same shape, selection and eraser vocabularies', async () => {
  const source = await (await import('node:fs/promises')).readFile(new URL('../../sdk/web/example.html', import.meta.url), 'utf8');
  for (const token of [
    'DEMO_SHAPES', 'DEMO_SELECTION_MODES', 'DEMO_ERASER_MODES',
    'DEMO_SHAPES.forEach', 'DEMO_SELECTION_MODES.map', 'DEMO_ERASER_MODES.map',
  ]) assert.match(source, new RegExp(token.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')));
});
