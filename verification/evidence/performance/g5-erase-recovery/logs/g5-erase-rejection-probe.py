import subprocess,json
p=subprocess.Popen(['out/host/sdk/common/axiom_native_transport_host'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
def call(x):
 p.stdin.write(json.dumps({'request':x})+'\n');p.stdin.flush();r=json.loads(p.stdout.readline());print(x.get('command'),x.get('phase',x.get('kind',x.get('action'))),{k:v for k,v in r.items()if k!='value'});return r
call({'command':'open','documentId':'9'*32,'builtinFonts':False})
call({'command':'data','action':'attach','capacity':1})
call({'command':'control','kind':'preset','profileId':'round-handdrawn','revision':4})
t=10000000
def pointer(phase,x,y):
 global t;t+=10000000;return call({'command':'pointer','source':1,'pointer':1,'phase':phase,'timestampNs':t,'x':x,'y':y})
pointer(0,40,100);pointer(1,400,100);pointer(2,400,100)
call({'command':'control','kind':'tool','value':2});call({'command':'control','kind':'eraser','mode':2,'diameter':30})
pointer(0,200,60);pointer(1,200,140);pointer(2,200,140)
r=call({'command':'state'});print('active',r.get('value',{}).get('activeContacts'))
pointer(0,220,60)
p.terminate();p.wait()
