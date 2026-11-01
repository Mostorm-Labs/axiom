// Application-side controls for the matching external_web WASM bridge.
// This preview wrapper does not promise a stable SDK ABI.
const tools = Object.freeze({ ink: [0, 0], select: [0, 1], partialEraser: [6, 2],
  objectEraser: [6, 1], hand: [0, 3], text: [0, 4] });
const shapes = Object.freeze({ rectangle: 0, ellipse: 1, triangle: 2, line: 3, arrow: 4, sticky: 5 });
const selectionModes = Object.freeze({ point: 0, rectangle: 1, lasso: 2 });

export function createCanvasControls({ module, beforeCommand = async () => {} }) {
  if (!module || typeof module._sdk_control !== 'function' ||
      typeof module._sdk_section !== 'function' || typeof module._sdk_schedule !== 'function' ||
      typeof beforeCommand !== 'function') throw new TypeError('Invalid external Web bridge');
  let tail = Promise.resolve();
  const submit = (kind, action, value) => {
    const task = tail.then(async () => {
      await beforeCommand();
      const accepted = kind === 'section' ? module._sdk_section(action) : module._sdk_control(action, value);
      if (!accepted) throw new Error('Canvas command rejected');
      module._sdk_schedule();
    });
    tail = task.catch(() => {});
    return task;
  };
  const lookup = (table, name, label) => {
    if (!Object.hasOwn(table, name)) throw new RangeError(`Unknown ${label}: ${name}`);
    return table[name];
  };
  return Object.freeze({
    control: (action, value) => submit('control', action, value),
    section: action => submit('section', action, 0),
    async selectTool(name) { const [action, value] = lookup(tools, name, 'tool'); return submit('control', action, value); },
    async selectShape(name) { return submit('control', 1, lookup(shapes, name, 'shape')); },
    async setSelectionMode(name) { return submit('control', 2, lookup(selectionModes, name, 'selection mode')); },
    undo: () => submit('control', 3, 0), redo: () => submit('control', 4, 0),
    fitContent: () => submit('control', 5, 0), createSection: () => submit('section', 0, 0),
    group: () => submit('section', 1, 0), ungroup: () => submit('section', 2, 0),
    enterGroup: () => submit('section', 3, 0), leaveGroup: () => submit('section', 4, 0),
  });
}
