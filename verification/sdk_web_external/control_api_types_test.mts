import { createCanvasControls } from './consumer/control_api.mjs';

const controls = createCanvasControls({
  module: { _sdk_control: () => 1, _sdk_section: () => 1, _sdk_schedule: () => {} },
  beforeCommand: async () => true,
});
await controls.selectTool('objectEraser');
await controls.selectShape('triangle');
await controls.setSelectionMode('lasso');
await controls.group();
// @ts-expect-error unknown tools must be rejected by the preview types
await controls.selectTool('not-a-tool');
// @ts-expect-error shape must be a known preset
await controls.selectShape(42);
