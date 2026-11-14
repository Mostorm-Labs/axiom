import {installPendingObserver} from '/g5/pending_navigation_progress_observer.mjs';
import {installNavigationObserver} from '/g5/navigation_observer.mjs';
import {run} from '/g5/pending_navigation_progress_consumer.mjs';
import {acknowledgeRow,verifyCompletion} from '/g5/pending_navigation_evidence_io_client.mjs';
installPendingObserver();window.installNav=installNavigationObserver;
const request=await(await fetch('/run-config.json')).json(),expected=request.rows.map(value=>value.lane+'-'+value.action);
try{
 let count=0;
 for(const {lane,action}of request.rows){
  document.getElementById('status').textContent='RUNNING '+lane+' '+action+' pixel=true';
  const row=await run({fixture:request.fixture,documentId:request.documentId,lane,action,pixel:true,sdkPrefix:'/lanes/'+lane+'/sdk/'});
  const response=await fetch('/result',{method:'POST',body:JSON.stringify(row)});
  await acknowledgeRow(response,lane+'-'+action,++count);
  if(row.status!=='OBSERVED')throw Error(row.failure);
 }
 const response=await fetch('/index.json');if(!response.ok)throw Error('index HTTP '+response.status);
 verifyCompletion(await response.json(),expected);
 document.getElementById('status').textContent='COMPLETE 4/4 durable pixel rows';
}catch(error){document.getElementById('status').textContent='FAILED '+String(error);await fetch('/failure',{method:'POST',body:String(error.stack??error)});}
