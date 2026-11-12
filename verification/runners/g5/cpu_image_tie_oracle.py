"""Independent PNG/filter + canonical-wire + exact-rational nearest ties only.
No renderer, candidate colour or candidate difference selects eligible pixels.
"""
from pathlib import Path
from fractions import Fraction as F
import struct,zlib,hashlib,json,math

def wire(b):
 at=0;out=[]
 def var():
  nonlocal at
  n=0
  for shift in range(0,70,7):
   if at>=len(b):raise ValueError('truncated varint')
   v=b[at];at+=1;n|=(v&127)<<shift
   if v<128:return n
  raise ValueError('wide varint')
 while at<len(b):
  tag=var();field,kind=tag>>3,tag&7
  if not field:raise ValueError('zero field')
  if kind==0:value=var()
  elif kind==1:value=b[at:at+8];at+=8
  elif kind==2:n=var();value=b[at:at+n];at+=n
  elif kind==5:value=b[at:at+4];at+=4
  else:raise ValueError('wire type')
  if at>len(b):raise ValueError('truncated wire')
  out.append((field,value))
 return out
def one(f,n,default=None):return next((v for k,v in f if k==n),default)
def d(b):return struct.unpack('<d',b)[0]if b else 0.
def f32(b):return struct.unpack('<f',b)[0]if b else 0.
def ident(b):return one(wire(b),1).hex()
def png_rgb(path):
 b=Path(path).read_bytes();assert b[:8]==b'\x89PNG\r\n\x1a\n';at=8;data=b'';w=h=0
 while at<len(b):
  n=struct.unpack('>I',b[at:at+4])[0];kind=b[at+4:at+8];p=b[at+8:at+8+n];at+=12+n
  if kind==b'IHDR':w,h,depth,colour,comp,filt,interlace=struct.unpack('>IIBBBBB',p);assert(depth,colour,comp,filt,interlace)==(8,2,0,0,0)
  if kind==b'IDAT':data+=p
 raw=zlib.decompress(data);stride=w*3;previous=bytearray(stride);rgb=bytearray()
 for y in range(h):
  kind=raw[y*(stride+1)];row=bytearray(raw[y*(stride+1)+1:(y+1)*(stride+1)])
  for i in range(stride):
   a=row[i-3]if i>=3 else 0;b=previous[i];c=previous[i-3]if i>=3 else 0
   if kind==1:p=a
   elif kind==2:p=b
   elif kind==3:p=(a+b)//2
   elif kind==4:
    v=a+b-c;dist=[abs(v-a),abs(v-b),abs(v-c)];p=[a,b,c][dist.index(min(dist))]
   elif kind==0:p=0
   else:raise ValueError('filter')
   row[i]=(row[i]+p)&255
  rgb+=row;previous=row
 return w,h,bytes(rgb)
def object_value(b):
 f=wire(b);content=wire(one(f,7));field,value=content[0];c=wire(value);t=wire(one(f,5,b''));pose=[d(one(t,n))for n in range(1,7)];props={one(wire(p),1):wire(one(wire(p),2))for k,p in wire(one(f,6,b''))if k==1};o={'id':ident(one(f,1)),'kind':one(f,2),'contentField':field,'pose':pose,'properties':props,'parent':one(wire(one(f,4,b'')),1),'order':one(wire(one(wire(one(f,4,b'')),2,b'')),1,b''),'opacity':f32(one(props.get(3,[]),2))if 3 in props else 1.}
 if field==2:o['image']={'resource':ident(one(c,1)),'iw':d(one(c,2)),'ih':d(one(c,3)),'mode':one(c,5),'width':d(one(c,6)),'height':d(one(c,7)),'crop':[d(one(wire(one(c,4)),n))for n in range(1,5)]if one(c,4)else[0.,0.,1.,1.]};o['bounds']=[0.,0.,o['image']['width'],o['image']['height']]
 elif field in [1,8]:o['bounds']=[0.,0.,d(one(c,2 if field==1 else 1)),d(one(c,3 if field==1 else 2))]
 elif field==13:o['bounds']=[0.,0.,sum(d(one(wire(v),2))for k,v in c if k==2),sum(d(one(wire(v),2))for k,v in c if k==1)]
 elif field==14:
  config=wire(one(c,1));pad=d(one(config,9))*2+8;p=one(c,4);points=list(struct.iter_unpack('<dd',p));o['bounds']=[min(x for x,y in points)-pad,min(y for x,y in points)-pad,max(x for x,y in points)+pad,max(y for x,y in points)+pad]
 else:o['bounds']=None
 # Account conservative property stroke width (no candidate pixels).
 stroke=one(props.get(0x101,[]),5)
 if stroke:
  solid=one(wire(stroke),2)
  if solid and o['bounds']:
   pad=d(one(wire(solid),2));l,t,r,b=o['bounds'];o['bounds']=[l-pad,t-pad,r+pad,b+pad]
 return o
def snapshot(path):return[object_value(v)for k,v in wire(Path(path).read_bytes())if k==3]
def world_bounds(o):
 if not o['bounds']:return None
 a,b,c,d_,tx,ty=o['pose'];l,t,r,z=o['bounds'];points=[(a*x+c*y+tx,b*x+d_*y+ty)for x in[l,r]for y in[t,z]];return[min(x for x,y in points),min(y for x,y in points),max(x for x,y in points),max(y for x,y in points)]
def indices(q,lo,hi):
 if q.denominator==1:
  return sorted(set(max(lo,min(hi,v))for v in[int(q)-1,int(q)]))
 return[max(lo,min(hi,q.numerator//q.denominator))]
def plan(image,camera,width,height,lw,lh,sourceWidth,sourceHeight):
 # Axis-aligned exact IEEE rational affine. Unsupported cases fail closed.
 a,b,c,dy,tx,ty=map(F,image['pose']);assert b==c==0 and a>0 and dy>0 and not image['parent']
 im=image['image'];sx,sy,sw,sh=map(F,im['crop']);sx*=sourceWidth;sy*=sourceHeight;sw*=sourceWidth;sh*=sourceHeight;dw,dh=F(im['width']),F(im['height']);left=top=F(0)
 if im['mode']in[1,2]:
  scale=(min if im['mode']==1 else max)(dw/sw,dh/sh);nw,nh=sw*scale,sh*scale;left=(dw-nw)/2;top=(dh-nh)/2;dw,dh=nw,nh
 zoom,cx,cy=F(camera['scale']),F(camera['x']),F(camera['y']);px=F(width)/F(lw);py=F(height)/F(lh)
 xs={};ys={}
 for x in range(width):
  world=((F(2*x+1,2)/px-cx)/zoom);local=(world-tx)/a
  if max(F(0),left)<local<min(F(im['width']),left+dw):
   q=sx+(local-left)*sw/dw
   if sx<=q<sx+sw:xs[x]=(world,q,indices(q,math.floor(sx),math.ceil(sx+sw)-1))
 for y in range(height):
  world=((F(2*y+1,2)/py-cy)/zoom);local=(world-ty)/dy
  if max(F(0),top)<local<min(F(im['height']),top+dh):
   q=sy+(local-top)*sh/dh
   if sy<=q<sy+sh:ys[y]=(world,q,indices(q,math.floor(sy),math.ceil(sy+sh)-1))
 return xs,ys
class TieOracle:
 def __init__(self,objects,camera,width,height,lw,lh,resource):
  self.objects=objects;self.width=width;self.height=height;self.camera=camera;self.lw=lw;self.lh=lh;self.rw,self.rh,self.rgb=png_rgb(resource);self.resourceId=hashlib.sha256(Path(resource).read_bytes()).hexdigest()[:32];self.images=[]
  ordered=sorted(objects,key=lambda o:(o['kind']in[5,6],o['order'],o['id']))
  for at,o in enumerate(ordered):
   if 'image'not in o or o['opacity']!=1 or o['image']['resource']!=self.resourceId:continue
   if o['kind']!=2 or o['image']['iw']!=self.rw or o['image']['ih']!=self.rh or o['image']['mode']not in[1,2,3]:continue
   if 1 in o['properties']and one(o['properties'][1],1)!=1:continue
   crop=o['image']['crop']
   if not(0<=crop[0]<1 and 0<=crop[1]<1 and 0<crop[2]<=1 and 0<crop[3]<=1 and crop[0]+crop[2]<=1 and crop[1]+crop[3]<=1):continue
   # Reject unknown later geometry. Conservative independent transformed AABBs
   # suffice for this literal fixture: strokes left, image right, root forest.
   if any(v['parent']or world_bounds(v)is None for v in ordered[at+1:]):continue
   try:xs,ys=plan(o,camera,width,height,lw,lh,self.rw,self.rh)
   except AssertionError:continue
   self.images.append((o,xs,ys,[world_bounds(v)for v in ordered[at+1:]]))
 def allowed(self,x,y,only_ties=True):
  for o,xs,ys,occluders in reversed(self.images):
   if x not in xs or y not in ys:continue
   wx,qx,ix=xs[x];wy,qy,iy=ys[y]
   if any(l-1<=float(wx)<=r+1 and t-1<=float(wy)<=b+1 for l,t,r,b in occluders):continue
   tie=qx.denominator==1 or qy.denominator==1
   if only_ties and not tie:continue
   values=[]
   for yy in iy:
    for xx in ix:
     p=(yy*self.rw+xx)*3;values.append(tuple(self.rgb[p:p+3])+(255,))
   return {'id':o['id'],'x':str(qx),'y':str(qy),'tie':tie,'indices':[[xx,yy]for yy in iy for xx in ix],'values':values}
  return None
 def compare(self,actual,expected,edge,hud=None):
  assert len(actual)==len(expected)==self.width*self.height*4 and len(edge)==self.width*self.height
  failures=[];failure_count=0;ties=0;ambiguous=0;corrected=0;strict=0;witness=[]
  # Only tie coordinates are considered for allowed expected values, independent
  # of actual or CPU colours. Non-ties retain original full-reference test.
  allowed={}
  for image,xs,ys,_ in self.images:
   for y,(_,qy,iy)in ys.items():
    for x,(_,qx,ix)in xs.items():
     if qx.denominator!=1 and qy.denominator!=1:continue
     i=y*self.width+x
     if edge[i]or(hud and hud[i]):continue
     v=self.allowed(x,y)
     if v:allowed[i]=v;ties+=1;ambiguous+=len(v['values'])>1
  for i in range(self.width*self.height):
   if edge[i]or(hud and hud[i]):continue
   a=actual[4*i:4*i+4];b=expected[4*i:4*i+4]
   delta=max(abs(a[k]-b[k])for k in range(4))
   if i in allowed:
    v=allowed[i];valid=any(max(abs(a[k]-c[k])for k in range(4))<=2 for c in v['values'])
    if valid and delta>2:
     corrected+=1
     if len(witness)<8:witness.append({'pixel':[i%self.width,i//self.width],'source':[v['x'],v['y']],'indices':v['indices'],'choices':v['values'],'actual':list(a),'CPU':list(b)})
   else:strict+=1;valid=delta<=2
   if not valid:
    failure_count+=1
    if len(failures)<8:failures.append({'pixel':[i%self.width,i//self.width],'actual':list(a),'CPU':list(b),'tie':i in allowed})
  # Full count in a second bounded pass only for failed rows not needed here.
  return {'status':'QUALIFIED'if not failures else'FAILED','unexplained_pixels':failure_count,'ties_considered':ties,'multi_choice_pixels':ambiguous,'corrected_CPU_disagreements':corrected,'strict_nontie_pixels':strict,'unexplained_witnesses':failures,'witnesses':witness}
