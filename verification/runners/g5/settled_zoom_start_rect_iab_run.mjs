import{installIdleObserver}from'/observer.mjs';import{installNavigationObserver}from'/navigation-observer.mjs';import{run}from'/consumer.mjs';
installIdleObserver();window.installNav=installNavigationObserver;
const config=await(await fetch('/run-config.json')).json();
const cases=[{name:'outside-one-stroke',count:1,points:1600,edit:'draw',ampX:900,ampY:600},{name:'outside-three-strokes-masked',count:3,points:1600,edit:'erase',ampX:900,ampY:600},{name:'central-short',count:1,points:256,edit:'draw',ampX:300,ampY:45}];
try{for(const pixel of[false,true])for(const lane of config.lanes)for(const c of cases){
 const restore=config.restores.find(v=>v.name===c.name&&v.pixel===pixel).snapshot;
 const q={...c,dpr:devicePixelRatio,fresh:true,preSettle:true,ratio:.5,wheelCount:24,pixel,restore,lane:lane.name,sdkPrefix:'/lanes/'+lane.name+'/sdk/'};
 document.getElementById('status').textContent='RUNNING '+lane.name+' '+q.name+' pixel='+pixel;
 const r=await run(q);document.getElementById('mount').replaceChildren();r.lane=lane.name;await fetch('/result',{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify(r)});if(r.status!=='OBSERVED')throw Error(r.failure);}
 document.getElementById('status').textContent='COMPLETE';}catch(e){document.getElementById('status').textContent='FAILED '+e;await fetch('/failure',{method:'POST',body:String(e.stack||e)});}
