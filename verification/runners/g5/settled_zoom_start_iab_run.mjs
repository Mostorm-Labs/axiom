import{installIdleObserver}from'/observer.mjs';import{installNavigationObserver}from'/navigation-observer.mjs';import{run}from'/consumer.mjs';
installIdleObserver();window.installNav=installNavigationObserver;
const cases=[{name:'outside-one-stroke',count:1,points:1600,edit:'draw',ampX:900,ampY:600},{name:'outside-three-strokes-masked',count:3,points:1600,edit:'erase',ampX:900,ampY:600},{name:'central-short',count:1,points:256,edit:'draw',ampX:300,ampY:45}];
const config=await(await fetch('/run-config.json')).json();
try{for(const pixel of[false,true])for(const c of cases){const mount=document.getElementById('mount');const old=[];for(const child of[...mount.children])old.push(child);for(const child of old)child.remove();
 const restored=config.restores?.find(v=>v.name===c.name&&v.pixel===pixel)?.snapshot;const q={...c,...(restored?{restore:restored}:{}),dpr:devicePixelRatio,fresh:true,preSettle:true,ratio:.5,wheelCount:24,pixel,version:'frozen'};document.getElementById('status').textContent='RUNNING '+q.name+' pixel='+pixel;
 const r=await run(q);await fetch('/result',{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify(r)});if(r.status!=='OBSERVED')throw Error(r.failure);}
 document.getElementById('status').textContent='COMPLETE';}catch(e){document.getElementById('status').textContent='FAILED '+e;await fetch('/failure',{method:'POST',body:String(e.stack||e)});}
