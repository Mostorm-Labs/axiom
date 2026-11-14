import {installPendingObserver} from '/g5/pending_navigation_progress_observer.mjs';
import {installNavigationObserver} from '/g5/navigation_observer.mjs';
import {run} from '/g5/pending_navigation_progress_consumer.mjs';
installPendingObserver();window.installNav=installNavigationObserver;
const q=await(await fetch('/run-config.json')).json();
try{
 for(const pixel of [false,true])for(const lane of q.lanes)for(const action of ['zoom','pan','nextdraw']){
  document.getElementById('status').textContent='RUNNING '+lane.name+' '+action+' pixel='+pixel;
  const row=await run({fixture:q.fixture,documentId:q.documentId,lane:lane.name,action,pixel,sdkPrefix:'/lanes/'+lane.name+'/sdk/'});
  await fetch('/result',{method:'POST',body:JSON.stringify(row)});if(row.status!=='OBSERVED')throw Error(row.failure);
 }
 document.getElementById('status').textContent='COMPLETE';
}catch(error){document.getElementById('status').textContent='FAILED '+String(error);await fetch('/failure',{method:'POST',body:String(error.stack??error)});}
