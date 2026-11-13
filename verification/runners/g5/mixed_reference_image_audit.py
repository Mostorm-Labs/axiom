from pathlib import Path
import json,hashlib,gzip,urllib.request
from mixed_reference_image import SourceImageAudit,f32_axis,rational,snapshot_file,source_point
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'out/G5-mixed-reference-qualification-r1';RESULT=ROOT/'.aegis/results/G5-mixed-reference-qualification-r1'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def run():
 workload=json.loads((ROOT/'.aegis/packages/G5-mixed-reference-qualification-r1/workload.json').read_text());row=workload['rows'][0]
 request=json.loads((ROOT/row['reference']['request.json']['path']).read_text());inputpath=OUT/row['row']/'input.axsnap';objects=snapshot_file(inputpath);image=ROOT/workload['image']['path'];audit=SourceImageAudit(objects,request['camera'],5724,4040,1431,1010,image)
 sources={};revision=json.loads((ROOT/'r1-full-skia-sdk.lock.json').read_text())['skia_commit']
 for name,lines in [('src/gpu/ganesh/geometry/GrQuad.cpp',[26,44]),('src/gpu/ganesh/ops/TextureOp.cpp',[99,181]),('src/gpu/ganesh/ops/QuadPerEdgeAA.cpp',[737,777]),('src/core/SkMatrix.cpp',[615,631]),('src/opts/SkRasterPipeline_opts.h',[3672,3685])]:
  local=ROOT/'.deps/skia'/name;data=local.read_bytes();status='UNBOUND_LOCAL_SOURCE'
  try:
   remote=urllib.request.urlopen('https://raw.githubusercontent.com/google/skia/'+revision+'/'+name,timeout=15).read();status='PINNED_SOURCE_MATCH'if remote==data else'LOCAL_DIFFERS_FROM_PIN';digest=hashlib.sha256(remote).hexdigest()
  except Exception as error:digest=None;status='PIN_FETCH_UNAVAILABLE:'+str(error)
  excerpt='\n'.join(data.decode().splitlines()[lines[0]-1:lines[1]])
  sources[name]={'local_sha256':sha(local),'pinned_sha256':digest,'status':status,'lines':lines,'excerpt':excerpt}
 # Enumerate the whole source image's device interior by independent predicate.
 # Points near/integral source coordinates are reported, not granted exemptions.
 sampled=[]
 obj=audit.images[0][0]
 for y in [3540,3541,3542]:
  qx,qy=source_point(obj,request['camera'],5724,4040,1431,1010,4011,y)
  sampled.append({'device':[4011,y],'source_x':rational(qx),'source_y':rational(qy),'tie':qx.denominator==1 or qy.denominator==1,'deterministic_rational_texel':[qx.numerator//qx.denominator,qy.numerator//qy.denominator],'f32Y':f32_axis(obj,request['camera'],4,1,y),'occlusion':'coordinate-onlydiagnostic; currentconservativebodyboundsincludeUp999andcannotqualifynewimageexpectedset'})
 result={'status':'UNPROVEN_REFERENCE_REALIZATION','image_SHA256':sha(image),'image_dimensions':[audit.w,audit.h],'image_semantic':obj,'camera':request['camera'],'device':[5724,4040],'source_analysis':sampled,'pinned_Skia_commit':revision,'source_excerpts':sources,'accepted_normalized_texture_path':'Device_drawTexture→drawTexture/TextureOp normalizeslocalquad by1/texturebackingdimensions→QuadPerEdgeAA varying→hardware nearest texture lookup','missing':['actualbackend choice TextureOp vsFillRect whencaps/AA','GPU varying andviewport precision/subtexel nearestselection domain at discontinuity','exactbackingdimensions/textureorigin realization from retainedframefacts'],'result':'No adjacenttexel alternatevalues applied. SourceY3541.5 is not exactrational tie. CPUillustrativef32 sequences remain below110; GPU choice cannot be proven from normalizedsource alone. Preserve302 unexplained.','new_browser':0,'new_fullscene_RGBA':0}
 (RESULT/'image-realization.json').write_text(json.dumps(result,indent=2,default=lambda v:v.hex()if isinstance(v,bytes)else str(v))+'\n');print(json.dumps({'status':result['status'],'source_status':{k:v['status']for k,v in sources.items()},'source_y':sampled[1]['source_y']}))
if __name__=='__main__':run()
