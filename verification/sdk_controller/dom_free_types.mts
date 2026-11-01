import type {Controller,ControllerOptions,InkBrushConfig,Revision} from '../../sdk/api/axiom.mjs';
function controls(c:Controller,options:ControllerOptions,brush:InkBrushConfig):Promise<Revision> {
 void options;void c.setBrushConfig(brush);return c.readState().then(s=>s.generation);
}
void controls;
