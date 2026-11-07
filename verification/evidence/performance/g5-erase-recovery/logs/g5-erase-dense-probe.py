import subprocess,json,time,math
p=subprocess.Popen(['out/host/sdk/common/axiom_native_transport_host'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
def call(x):
 a=time.perf_counter();p.stdin.write(json.dumps({'request':x})+'\n');p.stdin.flush();r=json.loads(p.stdout.readline());dt=(time.perf_counter()-a)*1000
 if not r.get('ok'):print(json.dumps({'stage':'rejected','request':x,'reply':r,'ms':dt}),flush=True)
 return r,dt
call({'command':'open','documentId':'9'*32,'builtinFonts':False});call({'command':'control','kind':'preset','profileId':'round-handdrawn','revision':4})
t=10000000
def pointer(phase,x,y):
 global t;t+=10000000;return call({'command':'pointer','source':1,'pointer':1,'phase':phase,'timestampNs':t,'x':x,'y':y})
for line in range(6):
 for i in range(1500):
  a=i*math.tau/55;pointer(0 if i==0 else 2 if i==1499 else 1,256+140*math.sin(a)+30*math.sin(a*.19)+line*3,256+110*math.cos(a*.67)+35*math.sin(a*.13))
 print(json.dumps({'stage':'stroke','line':line}),flush=True)
call({'command':'control','kind':'tool','value':2});call({'command':'control','kind':'eraser','mode':2,'diameter':28})
for n in range(6):
 times=[]
 for i in range(60):
  a=i*math.tau/17;x=256+100*math.sin(a)+n*3;y=256+90*math.cos(a*.7)
  r,dt=pointer(0 if i==0 else 2 if i==59 else 1,x,y);times.append(dt)
  if not r.get('ok'):break
 state,_=call({'command':'state'});print(json.dumps({'stage':'erase','n':n,'upMs':times[-1],'maxMoveMs':max(times[:-1],default=0),'ok':r.get('ok'),'active':state.get('value',{}).get('activeContacts'),'generation':state.get('value',{}).get('generation')}),flush=True)
 if not r.get('ok'):break
p.terminate();p.wait()
