export const CASES=['S0','S2'].flatMap(f=>['E-reerase','P-pan','Z-zoom-reentry','H-undo-before-present','B-brush-erase'].map(c=>f+'-'+c));
export function qualifyCold(rows){
  const failures=[];for(const stage of ['before','after','undo','redo']){
    const r=rows.find(r=>r.stage===stage);
    if(!r){failures.push(stage+':missing');continue;}
    if(r.errors?.length||!r.snapshot?.sha256)failures.push(stage+':capture-error');
    for(const role of ['canonical','preview'])if(!r.surfaces?.[role]?.sha256)failures.push(stage+':'+role+'-missing');
    if(r.oracle?.status!=='QUALIFIED')failures.push(stage+':oracle');
    const p=r.surfaces?.preview;
    if(p?.visible&&p.nonzeroAlpha>0)failures.push(stage+':visible-residual-preview');
  }
  return {status:failures.length?'FAILED':'QUALIFIED',failures};
}
export function contentFeedback(phase){
  const wheels=phase.inputEvents?.filter(e=>e.type==='wheel')??[];
  const start=wheels[0]?.started,end=wheels.at(-1)?.ended;
  const writes=(phase.colorOps??[]).filter(w=>w.kind==='content'&&w.started>=start&&w.ended<=end);
  const gaps=writes.slice(1).map((w,i)=>w.started-writes[i].ended);
  return {status:writes.length?'OBSERVED':'MISSING',content_writes:writes.length,maximum_content_gap_ms:gaps.length?Math.max(...gaps):null,
    first_feedback_ms:writes.length?writes[0].started-start:null,scope:'observed default-framebuffer color content operations during actual wheel input, not physical display'};
}
export function qualifyDisplayJoins(rows,tags){
  const records=new Map(),rosters=new Map(),headers=new Map(),submits=new Map(),qualified=new Map(),failures=[],outcomes=[];
  const workKey=v=>[v[1],v[2],v[3],v[4]].join('/');
  for(const r of rows){const v=r.values??[];
    if(r.tag===tags.display_record&&r.kind===6&&v.length===8)records.set(v[0],r);
    if(r.tag===tags.display_roster&&r.kind===5&&v.length===8)headers.set(v[0],r);
    if(r.tag===tags.display_roster&&r.kind===5&&v.length===2){const list=rosters.get(v[0])??new Set();list.add(v[1]);rosters.set(v[0],list);}
    if(r.tag===tags.display_roster&&r.kind===11&&v.length===6)submits.set(workKey(v),r);
    if(r.tag===tags.canonical_receipt&&r.kind===12&&v[0]==='1')qualified.set([v[2],v[3],v[4],v[1]].join('/'),r);
  }
  for(const r of rows){const v=r.values??[];if(r.tag!==tags.display_record||r.kind!==12)continue;
    const commit=records.get(v[0]),key=v.slice(2,6).join('/'),submit=submits.get(key),completion=qualified.get(key),serials=rosters.get(v[6]);
    const identity=commit&&commit.values.slice(2,6).join('/')===v.slice(7,11).join('/');
    const header=headers.get(v[6]);const frameBasis=header&&completion&&header.values.slice(1,5).join('/')===completion.values.slice(7,11).join('/');
    if(!commit||!submit||!completion||!serials?.has(v[0])||submit.values[0]!==v[6]||completion.values[6]!==v[6]||!identity||!frameBasis){failures.push({reason:'unqualified-or-foreign-display-outcome',row:r});continue;}
    outcomes.push({serial:v[0],outcome:['OwnCanonicalVisible','ReplacedByCurrentFrame','SupersededByCurrentState'][Number(v[1])]??'INVALID',work:key,frame:v[6],operation:v.slice(7,9),commit:v.slice(9,11),semantic:commit.values[6],own_visible_ns:v[1]==='0'?r.at_ns:null,replacement_ns:v[1]==='0'?null:r.at_ns});
  }
  return {status:failures.length?'INVALID':outcomes.length?'OBSERVED':'MISSING',outcomes,failures};
}
