/**
 * Shared product-demo control vocabulary.
 *
 * This module contains labels, ordering and stable action IDs only. Runtime
 * state and command execution remain owned by the SDK Controller.
 */
export const DEMO_BRUSHES = Object.freeze([
  Object.freeze({id: 'round', label: '圆头笔', profileId: 'round-handdrawn', revision: 4}),
  Object.freeze({id: 'marker', label: '马克笔', profileId: 'marker-compact-v1', revision: 4}),
  Object.freeze({id: 'chalk', label: '粉笔', profileId: 'chalk-compact-v1', revision: 3}),
  Object.freeze({id: 'crayon', label: '蜡笔', profileId: 'crayon-wax-v1', revision: 3}),
  Object.freeze({id: 'watercolor', label: '水彩笔', profileId: 'watercolor-lite-v1', revision: 3}),
  Object.freeze({id: 'laser', label: '激光笔', profileId: 'laser-transient', revision: 1}),
]);

export const DEMO_DRAWING_TOOLS = Object.freeze([
  Object.freeze({id: 'ink', label: '画笔', runtimeTool: 'ink', toolCode: 0}),
  Object.freeze({id: 'text', label: '文字', runtimeTool: 'text', toolCode: 4}),
  Object.freeze({id: 'objectEraser', label: '对象橡皮擦', runtimeTool: 'objectEraser', toolCode: 2}),
  Object.freeze({id: 'partialEraser', label: '局部橡皮擦', runtimeTool: 'partialEraser', toolCode: 2}),
  Object.freeze({id: 'select', label: '选择', runtimeTool: 'select', toolCode: 1}),
  Object.freeze({id: 'hand', label: '平移', runtimeTool: 'hand', toolCode: 3}),
]);

export const DEMO_BOARD_ACTIONS = Object.freeze([
  Object.freeze({id: 'undo', label: '撤销'}),
  Object.freeze({id: 'redo', label: '重做'}),
  Object.freeze({id: 'fit', label: '适配'}),
  Object.freeze({id: 'background', label: '背景'}),
  Object.freeze({id: 'export', label: '导出'}),
  Object.freeze({id: 'detach', label: '重挂载'}),
]);

export const DEMO_PRESSURE_SOURCES = Object.freeze([
  Object.freeze({id: 1, label: '模拟压力（默认）'}),
  Object.freeze({id: 2, label: '设备压力'}),
]);

export const DEMO_SHAPES = Object.freeze([
  Object.freeze({id: 'rectangle', label: '矩形'}),
  Object.freeze({id: 'ellipse', label: '椭圆'}),
  Object.freeze({id: 'triangle', label: '三角形'}),
  Object.freeze({id: 'line', label: '直线'}),
  Object.freeze({id: 'arrow', label: '连线'}),
  Object.freeze({id: 'sticky', label: '便签'}),
]);

export const DEMO_SELECTION_MODES = Object.freeze([
  Object.freeze({id: 'point', label: '点选'}),
  Object.freeze({id: 'rectangle', label: '框选'}),
  Object.freeze({id: 'lasso', label: '套索'}),
]);

export const DEMO_ERASER_MODES = Object.freeze([
  Object.freeze({id: 'objectEraser', label: '整个对象'}),
  Object.freeze({id: 'partialEraser', label: '局部擦除'}),
]);

export function resolveDemoBrushChoices(available = DEMO_BRUSHES) {
  const revisions = new Map();
  for (const preset of available ?? []) {
    if (!preset || typeof preset.profileId !== 'string' || !Number.isInteger(preset.revision)) continue;
    const previous = revisions.get(preset.profileId);
    if (previous === undefined || preset.revision > previous) revisions.set(preset.profileId, preset.revision);
  }
  return DEMO_BRUSHES
    .filter(brush => revisions.has(brush.profileId))
    .map(brush => Object.freeze({...brush, revision: revisions.get(brush.profileId)}));
}
