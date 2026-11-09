// Cold verification only. The injected persist function runs before any verdict.
export async function captureColdStage(stage,{readState,readSurface,snapshot,persist}){
  const row={stage,surfaces:{},errors:[]};
  try{Object.assign(row,await readState());}catch(e){row.errors.push({part:'state',message:String(e)});}
  for(const role of ['canonical','preview'])try{
    const image=await readSurface(role);const key=stage+'-'+role;
    const saved=await persist(key,image.bytes);
    row.surfaces[role]={...image,bytes:undefined,key,...saved};
  }catch(e){row.errors.push({part:role,message:String(e)});}
  try{const value=await snapshot(row);row.snapshot={...value,bytes:undefined,key:stage+'-snapshot',...await persist(stage+'-snapshot',value.bytes)};}
  catch(e){row.errors.push({part:'snapshot',message:String(e)});}
  return row;
}
export async function captureFirstSequence({capture,action,undo,redo}){
  const rows=[],errors=[];
  const save=async stage=>{try{rows.push(await capture(stage));}catch(e){errors.push({stage,message:String(e)});}};
  await save('before');
  try{await action();}catch(e){errors.push({stage:'action',message:String(e)});}
  await save('after');
  for(const [stage,command]of [['undo',undo],['redo',redo]]){try{await command();}catch(e){errors.push({stage,message:String(e)});}await save(stage);}
  return {rows,errors};
}
