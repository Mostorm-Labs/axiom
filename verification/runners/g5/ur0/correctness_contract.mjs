// Verification-only fixed fixture applicability. Never called by product input.
const id=x=>typeof x==='string'&&/^[0-9a-f]{32}$/.test(x);
const uint=x=>typeof x==='string'&&/^(0|[1-9][0-9]*)$/.test(x)&&BigInt(x)<2n**64n;
const near=(a,b)=>Array.isArray(a)&&a.length===b.length&&a.every((v,i)=>Number.isFinite(v)&&Math.abs(v-b[i])<=1e-4);
const same=(a,b)=>JSON.stringify(a)===JSON.stringify(b);
function bounds(b){if(!Array.isArray(b)||b.length!==4||b.some(v=>!Number.isFinite(v))||b[0]>b[2]||b[1]>b[3])throw Error('invalid public selection bounds');return b;}
function camera(c){if(!c||!Number.isFinite(c.scale)||c.scale<=0||!Number.isFinite(c.x)||!Number.isFinite(c.y))throw Error('invalid public camera');return c;}
const project=(b,c)=>[b[0]*c.scale+c.x,b[1]*c.scale+c.y,b[2]*c.scale+c.x,b[3]*c.scale+c.y];
export function fixedLiveExpectation(before){
 const s=before?.selection,state=before?.state;
 if(!s||s.count!==1||s.kind!==5||!id(s.primary)||!same(s.roots,[s.primary])||state?.primary!==s.primary||!same(state.selected,[s.primary])||state.objects!==2||!id(state.documentId)||!near(s.transform,[1,0,0,1,0,0]))throw Error('fixed lower-z selected stroke fixture not applicable');
 const b=bounds(s.bounds),c=camera(state.camera);if(c.scale!==1||c.x!==0||c.y!==0)throw Error('fixed fixture camera changed');
 const tx=24,ty=260-(b[1]+b[3])/2;
 return{stroke_id:s.primary,document_id:state.documentId,transform:[1,0,0,1,tx,ty],bounds:[b[0]+tx,b[1]+ty,b[2]+tx,b[3]+ty]};
}
export function selectionHudRequest({selection,camera:c,caseId,step,before}){
 camera(c);if(!selection||!Number.isSafeInteger(selection.count)||selection.count<0)throw Error('public selection missing');
 const hud_bounds=selection.count?[project(bounds(selection.bounds),c)]:[];
 const result={case_id:caseId,step,hud_bounds};
 if(caseId==='lower_z_translucent_drag'&&step===0){
  const expected=fixedLiveExpectation(before);result.hud_bounds=[project(bounds(before.selection.bounds),c),project(expected.bounds,c)];result.hud_guide_y=260;
 }
 return result;
}
export function validateLiveSubject({before,live,after,nativeBefore,nativeAfter,capturedBefore,capturedAfter}){
 const expected=fixedLiveExpectation(before),s=before.state,selected=expected.stroke_id;
 for(const [captured,row] of [[capturedBefore,before],[capturedAfter,after]])if(captured){
  if(['documentId','generation','objects','primary','selected','camera','target','activeContacts','lastCommit'].some(k=>!same(captured.state?.[k],row.state?.[k]))||['count','primary','roots','generation','bounds','transform','kind'].some(k=>!same(captured.selection?.[k],row.selection?.[k])))throw Error('opaque snapshot capture differs from live reference subject');
 }
 if(!uint(s.generation)||!uint(s.lastCommit?.ordinal)||s.activeContacts!==0)throw Error('invalid before state');
 const metricKeys=['width','height','logical_width','logical_height','dpr'];
 if(!before.metrics||metricKeys.some(k=>!Number.isFinite(before.metrics[k])||before.metrics[k]<=0))throw Error('missing public viewport/DPR metrics');
 for(const row of [live,after]){
  if(!row||row.state?.documentId!==s.documentId||row.state?.objects!==2||row.state?.primary!==selected||!same(row.state.selected,[selected])||row.selection?.count!==1||row.selection.primary!==selected||!same(row.selection.roots,[selected])||row.selection.kind!==5||!same(row.state.camera,s.camera)||!same(row.state.target,s.target)||metricKeys.some(k=>row.metrics?.[k]!==before.metrics[k]))throw Error('foreign/changed live subject, camera, target or DPR');
 }
 if(live.state.generation!==s.generation||live.selection.generation!==before.selection.generation||live.state.lastCommit?.ordinal!==s.lastCommit.ordinal||live.state.activeContacts!==1)throw Error('live frame mutated canonical state or lost contact');
 const a=after.state;if(!uint(a.generation)||BigInt(a.generation)!==BigInt(s.generation)+1n||!uint(a.lastCommit?.ordinal)||BigInt(a.lastCommit.ordinal)!==BigInt(s.lastCommit.ordinal)+1n||a.lastCommit.presentedGeneration!==a.generation||a.activeContacts!==0||!id(a.lastCommit.operationId)||/^0+$/.test(a.lastCommit.operationId)||a.lastCommit.operationId===s.lastCommit.operationId||after.selection.generation!==a.generation)throw Error('Up lacks exactly one completed canonical transform commit');
 if(!near(after.selection.transform,expected.transform)||!near(bounds(after.selection.bounds),expected.bounds))throw Error('public committed transform differs from independent fixed expectation');
 for(const [decoded,row] of [[nativeBefore,before],[nativeAfter,after]]){
  if(decoded?.status!=='DECODED'||decoded.document_revision!==row.state.generation||decoded.object_count!==2||!Array.isArray(decoded.objects)||decoded.objects.length!==2||decoded.objects.some(o=>!id(o.id))||new Set(decoded.objects.map(o=>o.id)).size!==2||decoded.objects[0].id!==selected||decoded.objects[0].kind!==5||decoded.objects[1].kind!==1)throw Error('native snapshot fixture identities/order invalid');
 }
 const body=o=>Object.fromEntries(Object.entries(o).filter(([k])=>k!=='transform'));
 if(!same(nativeBefore.objects.map(o=>[o.id,o.kind,o.kind_version]),nativeAfter.objects.map(o=>[o.id,o.kind,o.kind_version]))||!near(nativeBefore.objects[0].transform,[1,0,0,1,0,0])||!near(nativeAfter.objects[0].transform,expected.transform)||!same(body(nativeBefore.objects[0]),body(nativeAfter.objects[0]))||!same(nativeBefore.objects[1],nativeAfter.objects[1]))throw Error('native snapshot transform/order/body differs from independent expectation');
 return{status:'QUALIFIED',expected_transform:expected.transform,selected_stroke:selected,object_order:nativeBefore.objects.map(o=>o.id),reference:'actual after-Up opaque snapshot; fixed expectation independent of candidate'};
}
