"""Compare ON/OFF captured final pixels; never infer equality from timing."""
import json,sys
from pathlib import Path
on=json.loads((Path(sys.argv[1])/'observation.json').read_text())
off=json.loads((Path(sys.argv[2])/'observation.json').read_text())
assert (on['count'],on['dpr'],on['reverse'])==(off['count'],off['dpr'],off['reverse'])
assert on['timingRows'] and not off['timingRows'], 'incorrect diagnostic profiles'
assert on['image']==off['image'], 'render output or renderer differs'
assert on['raw']['after']['generation']==off['raw']['after']['generation']
assert on['raw']['after']['objects']==off['raw']['after']['objects']
assert not on['raw']['errors'] and not off['raw']['errors']
print(f"ON/OFF final RGBA and canonical counts equal: {on['count']} strokes DPR{on['dpr']}")
