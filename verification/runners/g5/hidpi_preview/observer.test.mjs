import test from 'node:test';
import assert from 'node:assert/strict';
import {installPreviewObserver} from './observer.mjs';
test('preview observer excludes offscreen, masked and noncolor writes without scheduling',()=>{
 let at=0,requested=0;const callbacks=new Map(),canvas={};
 class GL {constructor(){this.canvas=canvas;this.FRAMEBUFFER=1;this.DRAW_FRAMEBUFFER=2;this.COLOR_BUFFER_BIT=4;}}
 for(const name of ['bindFramebuffer','colorMask','drawArrays','drawElements','drawArraysInstanced','drawElementsInstanced','blitFramebuffer'])GL.prototype[name]=()=>{};
 const env={WebGL2RenderingContext:GL,performance:{now:()=>++at},console:{log(){},error(){},warn(){},info(){}},requestAnimationFrame(fn){callbacks.set(++requested,fn);return requested;},cancelAnimationFrame(id){callbacks.delete(id);}};
 const original=GL.prototype.drawArrays,observer=installPreviewObserver(()=>canvas,env),gl=new GL();observer.begin();
 gl.bindFramebuffer(1,{});gl.drawArrays();gl.bindFramebuffer(2,null);gl.colorMask(false,false,false,false);gl.drawArrays();
 gl.colorMask(true,true,true,true);gl.blitFramebuffer(0,0,0,0,0,0,0,0,0);gl.drawElements();
 const handle=env.requestAnimationFrame(()=>gl.drawArrays());assert.equal(observer.pending(),1);callbacks.get(handle)(1);assert.equal(observer.pending(),0);
 const row=observer.end();assert.equal(requested,1);assert.equal(row.writes.length,2);assert.equal(row.callbacks.length,1);assert.equal(row.dropped,0);
 observer.restore();assert.equal(GL.prototype.drawArrays,original);
});
test('input measurements preserve event order and exceptions',()=>{
 let at=0;class GL{};const env={WebGL2RenderingContext:GL,performance:{now:()=>++at},console:{},requestAnimationFrame(){throw Error('observer must not schedule');},cancelAnimationFrame(){}};
 const observer=installPreviewObserver(()=>null,env);observer.begin();const target={dispatchEvent(e){if(e.type==='bad')throw Error('dispatch');return true;}};
 observer.dispatch(target,{type:'pointerdown'},0);assert.throws(()=>observer.dispatch(target,{type:'bad'},1),/dispatch/);
 const rows=observer.end().inputs;assert.deepEqual(rows.map(v=>v.index),[0,1]);assert.ok(rows.every(v=>v.ended>=v.started));observer.restore();
});
