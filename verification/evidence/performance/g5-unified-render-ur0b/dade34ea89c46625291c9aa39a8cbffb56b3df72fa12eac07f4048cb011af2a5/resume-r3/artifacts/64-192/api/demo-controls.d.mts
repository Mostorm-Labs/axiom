export interface DemoBrush {
  readonly id: 'round'|'marker'|'chalk'|'crayon'|'watercolor'|'laser';
  readonly label: string;
  readonly profileId: string;
  readonly revision: number;
}
export interface DemoTool {readonly id:string; readonly label:string; readonly runtimeTool:string; readonly toolCode:number;}
export interface DemoBoardAction {readonly id:string; readonly label:string;}
export interface DemoPressureSource {readonly id:1|2; readonly label:string;}
export interface DemoChoice {readonly id:string; readonly label:string;}
export const DEMO_BRUSHES: readonly DemoBrush[];
export const DEMO_DRAWING_TOOLS: readonly DemoTool[];
export const DEMO_BOARD_ACTIONS: readonly DemoBoardAction[];
export const DEMO_PRESSURE_SOURCES: readonly DemoPressureSource[];
export const DEMO_SHAPES: readonly DemoChoice[];
export const DEMO_SELECTION_MODES: readonly DemoChoice[];
export const DEMO_ERASER_MODES: readonly DemoChoice[];
export function resolveDemoBrushChoices(available?: readonly {profileId:string;revision:number}[]): DemoBrush[];
