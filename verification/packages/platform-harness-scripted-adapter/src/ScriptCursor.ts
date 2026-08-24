import type { ScriptProgram, ScriptStep } from "./ScriptProgram.js";
export class ScriptCursor { private index = 0; constructor(private readonly program: ScriptProgram) {} next(): ScriptStep | null { return this.index < this.program.steps.length ? this.program.steps[this.index++]! : null; } get done(): boolean { return this.index === this.program.steps.length; } }
