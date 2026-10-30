/** Preview application wrapper for the matching external_web WASM bridge. */
export type CanvasTool = 'ink' | 'select' | 'partialEraser' | 'objectEraser' | 'hand' | 'text';
export type CanvasShape = 'rectangle' | 'ellipse' | 'triangle' | 'line' | 'arrow' | 'sticky';
export type SelectionMode = 'point' | 'rectangle' | 'lasso';
export interface CanvasBridge {
  _sdk_control(action: number, value: number): number;
  _sdk_section(action: number): number;
  _sdk_schedule(): void;
}
export interface CanvasControls {
  control(action: number, value: number): Promise<void>;
  section(action: number): Promise<void>;
  selectTool(name: CanvasTool): Promise<void>;
  selectShape(name: CanvasShape): Promise<void>;
  setSelectionMode(name: SelectionMode): Promise<void>;
  undo(): Promise<void>;
  redo(): Promise<void>;
  fitContent(): Promise<void>;
  createSection(): Promise<void>;
  group(): Promise<void>;
  ungroup(): Promise<void>;
  enterGroup(): Promise<void>;
  leaveGroup(): Promise<void>;
}
export function createCanvasControls(options: {
  module: CanvasBridge;
  beforeCommand?: () => unknown | Promise<unknown>;
}): CanvasControls;
