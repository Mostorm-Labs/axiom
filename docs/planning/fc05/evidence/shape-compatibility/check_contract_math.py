"""Contract-only analytical check; never calls a production resolver."""
from fractions import Fraction as F
import hashlib
import json
from pathlib import Path

def cross(a,b): return a[0]*b[1]-a[1]*b[0]
def sub(a,b): return (a[0]-b[0],a[1]-b[1])
def edge_reference(w,h,d):
    c=(w/2,h/2); v=((w/2,F(0)),(w,h),(F(0),h))
    hits=[]
    for a,b in zip(v,v[1:]+v[:1]):
        e=sub(b,a); den=cross(d,e)
        if not den: continue
        t=cross(sub(a,c),e)/den; u=cross(sub(a,c),d)/den
        if t>=0 and 0<=u<=1: hits.append(t)
    if not hits: raise AssertionError('reference ray has no boundary intersection')
    t=min(hits)
    return (c[0]+t*d[0],c[1]+t*d[1])

def half_plane_contract(w,h,d):
    q=(d[0]/w,d[1]/h); times=[]
    a=2*q[0]+q[1]; b=-2*q[0]+q[1]
    if a<0: times.append(-F(1,2)/a)
    if b<0: times.append(-F(1,2)/b)
    if q[1]>0: times.append(F(1,2)/q[1])
    t=min(times)
    return (w*(F(1,2)+t*q[0]),h*(F(1,2)+t*q[1]))

def main():
    count=0
    for width,height in [(200,120),(128,128),(80,240),(3,7)]:
        w,h=F(width),F(height)
        for dx in range(-4,5):
            for dy in range(-4,5):
                if dx==dy==0: continue
                d=(F(dx),F(dy))
                actual=half_plane_contract(w,h,d)
                assert actual==edge_reference(w,h,d),(w,h,d,actual)
                x,y=actual
                assert 0<=x<=w and 0<=y<=h
                assert 2*x/w+y/h>=1 and -2*x/w+y/h>=-1 and y/h<=1
                count+=1
        expected=[(w/2,F(0)),(3*w/4,h/2),(w/2,h),(w/4,h/2)]
        for d,p in zip([(F(0),F(-1)),(F(1),F(0)),(F(0),F(1)),(F(-1),F(0))],expected):
            assert edge_reference(w,h,d)==p
            count+=1
    assert half_plane_contract(F(200),F(120),(F(100),F(60)))==(F(200),F(120))
    count+=1
    contract=Path(__file__).resolve().parents[3]/'FC05_CONNECTOR_SHAPE_COMPATIBILITY_CONTRACT.md'
    result={'classification':'contract math evidence; not product Runtime or platform PASS',
            'cases':count,'result':'PASS','numeric_oracle':'exact rational vertex/edge ray intersection',
            'contract_sha256':hashlib.sha256(contract.read_bytes()).hexdigest()}
    print(json.dumps(result,indent=2))

if __name__=='__main__': main()
