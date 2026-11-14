import {installPendingObserver} from '/g5/pending_navigation_progress_observer.mjs';
import {installNavigationObserver} from '/g5/navigation_observer.mjs';
import {run} from '/g5/pending_navigation_held_worker_r2_consumer.mjs';
import {acknowledgeRow,verifyCompletion} from '/g5/pending_navigation_evidence_io_client.mjs';
installPendingObserver();window.installNav=installNavigationObserver;
document.getElementById('status').textContent='RUNNING controlled real Worker terminal delay (qualification only)';
try{
 const q=await(await fetch('/run-config.json')).json();
 const row=await run({...q,q:undefined,lane:'candidate',action:'heldzoom',pixel:true,sdkPrefix:'/lanes/candidate/sdk/'});
 const response=await fetch('/result',{method:'POST',body:JSON.stringify(row)});await acknowledgeRow(response,'candidate-heldzoom',1);
 if(row.status!=='OBSERVED')throw Error(row.failure);
 const index=await fetch('/index.json');if(!index.ok)throw Error('indexHTTP'+index.status);verifyCompletion(await index.json(),['candidate-heldzoom']);
 document.getElementById('status').textContent='COMPLETE r2 real Worker proof 1/1 durable';
}catch(error){document.getElementById('status').textContent='FAILED '+String(error);await fetch('/failure',{method:'POST',body:String(error.stack??error)});}
