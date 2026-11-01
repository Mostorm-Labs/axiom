import type {Controller,InkBrushConfig} from '../../sdk/api/axiom.mjs';
import {createController} from '../../sdk/web/axiom.mjs';
async function controls(c:Controller) {
 const state=await c.readState();
 const id:string=state.documentId;
 const config:Required<InkBrushConfig>|null=await c.readBrushConfig();
 const selected=await c.readSelection();
 await c.selectTool('ink'); await c.selectBrush('crayon-wax-v1',3);
 await c.undo();await c.redo();await c.resources.readBytes((await c.resources.token(id))!);
 const off=c.subscribeState(s=>{const generation:string=s.generation;void generation;});off();
 // @ts-expect-error Native/UI consumers cannot access Web surface internals.
 c.canvas;
 // @ts-expect-error Reads require awaiting the owner thread.
 const sync:string=c.readState().generation;
 void [config,selected,sync];
}
void controls;void createController;
