// Functional exercise only. SDK RAF/native records independently prove drawing.
export function boundedDisplayOpportunity({timeoutMs=2000,signal,
  request=requestAnimationFrame,cancel=cancelAnimationFrame,delay=setTimeout,clear=clearTimeout}={}){
  return new Promise((resolve,reject)=>{
    let frame=0,timer,done=false;
    const finish=error=>{if(done)return;done=true;if(error&&frame)cancel(frame);clear(timer);
      signal?.removeEventListener('abort',abort);error?reject(error):resolve();};
    const abort=()=>finish(Error('functional display aborted'));
    if(signal?.aborted){abort();return;}
    if(!Number.isFinite(timeoutMs)||timeoutMs<=0){finish(Error('functional display opportunity timeout'));return;}
    signal?.addEventListener('abort',abort,{once:true});
    timer=delay(()=>finish(Error('functional display opportunity timeout')),timeoutMs);
    const callback=()=>finish();callback.g5SamplerRaf=true;
    try{frame=request(callback);}catch(error){finish(error);}
  });
}

export async function runFunctionalErase({down,move,up,cancel,position,schedule,opportunities,
  now,until,display,signal,budgetMs=15000}){
  const entered=now();let contact=false;
  const bounded=()=>{if(signal?.aborted)throw Error('functional erase aborted');
    if(!Number.isFinite(budgetMs)||budgetMs<=0||now()-entered>budgetMs)throw Error('functional erase budget exceeded');};
  const opportunity=async(after,i)=>{bounded();const started=now();
    await display({signal,timeoutMs:budgetMs-(started-entered)});
    opportunities.push({after,i,started,ended:now()});bounded();};
  try{
    bounded();contact=true;down();const start=now();
    await opportunity('down',0);
    for(let i=1;i<=32;++i){const planned=start+i*16.666667;await until(planned);bounded();
      const actual=now();move(...position(i));schedule.push({event:'move',i,planned,actual});
      // Overdue deadlines must still yield an actual browser display opportunity.
      await opportunity('move',i);
    }
    bounded();up();contact=false;
  }catch(error){if(contact)cancel();throw error;}
}
