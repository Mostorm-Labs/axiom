"""Source-only image arithmetic audit. No backend ambiguity is accepted here.
Exact rational diagnostics and explicit f32 sequences are distinct from a proven
GPU varying/sampler realization. Actual colors are read only by final checks.
"""
from pathlib import Path
from fractions import Fraction as Q
import math,struct,hashlib,ctypes,gzip
from cpu_image_tie_oracle import object_value,wire
from cpu_image_tie_oracle import png_rgb,snapshot,world_bounds

def snapshot_file(path):
 path=Path(path);data=path.read_bytes()if path.exists()else gzip.decompress(Path(str(path)+'.gz').read_bytes())
 return [object_value(v)for k,v in wire(data)if k==3]
def f32(value):return struct.unpack('<f',struct.pack('<f',float(value)))[0]
def fma32(a,b,c):
 # Exact IEEE single fused multiply/add rounded once, using host libm.
 lib=ctypes.CDLL(None);f=lib.fmaf;f.argtypes=[ctypes.c_float]*3;f.restype=ctypes.c_float
 return f(a,b,c)
def rational(value):return {'value':float(value),'numerator':str(value.numerator),'denominator':str(value.denominator)}
def source_point(image,camera,width,height,lw,lh,x,y):
 if image['parent']or image['opacity']!=1 or image['pose'][1]!=0 or image['pose'][2]!=0 or image['pose'][0]<=0 or image['pose'][3]<=0:raise ValueError('unsupported affine/parent/opacity')
 im=image['image'];a,_,_,d,tx,ty=map(Q,image['pose']);zoom,cx,cy=map(Q,[camera['scale'],camera['x'],camera['y']]);
 wx=(Q(2*x+1,2)/(Q(width)/Q(lw))-cx)/zoom;wy=(Q(2*y+1,2)/(Q(height)/Q(lh))-cy)/zoom
 lx=(wx-tx)/a;ly=(wy-ty)/d
 sx,sy,sw,sh=map(Q,im['crop']);sx*=int(im['iw']);sy*=int(im['ih']);sw*=int(im['iw']);sh*=int(im['ih']);dw,dh=map(Q,[im['width'],im['height']]);left=top=Q(0)
 if im['mode']in[1,2]:
  scale=(min if im['mode']==1 else max)(dw/sw,dh/sh);nw,nh=sw*scale,sh*scale;left=(dw-nw)/2;top=(dh-nh)/2;dw,dh=nw,nh
 if not(max(Q(0),left)<lx<min(Q(im['width']),left+dw)and max(Q(0),top)<ly<min(Q(im['height']),top+dh)):raise ValueError('outside opaque image interior')
 return sx+(lx-left)*sw/dw,sy+(ly-top)*sh/dh
class SourceImageAudit:
 def __init__(self,objects,camera,width,height,lw,lh,resource):
  self.objects=objects;self.camera=camera;self.width=width;self.height=height;self.lw=lw;self.lh=lh;self.resource=Path(resource)
  self.w,self.h,self.rgb=png_rgb(resource);self.identity=hashlib.sha256(self.resource.read_bytes()).hexdigest()[:32]
  self.images=[]
  ordered=sorted(objects,key=lambda o:(o['kind']in[5,6],o['order'],o['id']))
  for i,o in enumerate(ordered):
   if'image'not in o or o['image']['resource']!=self.identity:continue
   if o['image']['iw']!=self.w or o['image']['ih']!=self.h:continue
   if o['opacity']!=1 or o['parent']or o['pose'][1]!=0 or o['pose'][2]!=0:continue
   self.images.append((o,[world_bounds(x)for x in ordered[i+1:]]))
 def diagnostic_sample(self,x,y):
  # Diagnostic rational prediction only, never alternate acceptance against oldRGBA.
  for image,occluders in self.images:
   try:qx,qy=source_point(image,self.camera,self.width,self.height,self.lw,self.lh,x,y)
   except ValueError:continue
   a,b,c,d,tx,ty=map(Q,image['pose']);im=image['image'];wx=(Q(2*x+1,2)/(Q(self.width)/Q(self.lw))-Q(self.camera['x']))/Q(self.camera['scale']);wy=(Q(2*y+1,2)/(Q(self.height)/Q(self.lh))-Q(self.camera['y']))/Q(self.camera['scale'])
   if any(v is None or v[0]-1<=float(wx)<=v[2]+1 and v[1]-1<=float(wy)<=v[3]+1 for v in occluders):continue
   ix,iy=math.floor(qx),math.floor(qy)
   if not(0<=ix<self.w and 0<=iy<self.h):continue
   at=(iy*self.w+ix)*3
   return {'source_x':qx,'source_y':qy,'texel':[ix,iy],'value':tuple(self.rgb[at:at+3])+(255,),'tie':qx.denominator==1 or qy.denominator==1}
  return None
 def qualified_alternate_values(self,x,y):
  # Missing exact GPU interpolation/sampler precision is a real proof gap.
  # Never turn adjacent PNG colors or a near-integer coordinate into permission.
  return None

def f32_axis(image,camera,device_scale,axis,pixel):
 im=image['image'];size=int(im['iw']if axis==0 else im['ih']);logical=im['width']if axis==0 else im['height'];pose=image['pose'][4+axis];translation=camera['x']if axis==0 else camera['y']
 z=f32(camera['scale']);scale=f32(device_scale*z);offset=f32(device_scale*f32(f32(z*f32(pose))+f32(translation)))
 fit=f32(logical/size);combined=f32(scale*fit);inverse=f32(1/combined);inverse_translation=f32(-offset*inverse)
 center=f32(pixel+.5);separate=f32(f32(center*inverse)+inverse_translation);fused=fma32(center,inverse,inverse_translation)
 top=offset;bottom=f32(f32(scale*f32(logical))+offset);local=f32(f32(center-top)/f32(bottom-top));normalized=f32(local*f32(f32(size*f32(1/size))))
 return {'device_scale':scale,'device_offset':offset,'Fit_f32':fit,'source_to_device_scale':combined,'inverse_scale':inverse,'inverse_translate':inverse_translation,'CPU_illustrative_unfused':separate,'CPU_illustrative_fma':fused,'device_quad_endpoints':[top,bottom],'normalized_interpolated_illustrative':normalized,'normalized_times_dimension':f32(normalized*size),'notes':'These exact f32 sequences are diagnostic possibilities; GPU rasterizer interpolation/viewport/subtexel quantization precision not identified. No expectedset follows.'}
