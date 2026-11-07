from pathlib import Path
import subprocess, struct, array, json, sys, shutil
root = Path(sys.argv[1]); ffmpeg = shutil.which('ffmpeg')
def decode(name):
    path = root / name
    width, height = struct.unpack('>II', path.read_bytes()[16:24])
    data = subprocess.check_output([ffmpeg, '-v', 'error', '-i', str(path), '-f', 'rawvideo', '-pix_fmt', 'rgba', 'pipe:1'])
    assert len(data) == width * height * 4
    return width, height, data
w, h, a = decode('cpu.png'); bw, bh, b = decode('gpu.png'); assert (w, h) == (bw, bh)
x = array.array('I', a); y = array.array('I', b); horizontal = bytearray(w * h)
for row in range(h):
    for col in range(2, w - 2):
        i = row * w + col; c = x[i]
        horizontal[i] = x[i-2] == x[i-1] == c == x[i+1] == x[i+2]
flat = bad = flat_max = all_max = changed = over = 0
for i, (c, v) in enumerate(zip(x, y)):
    col = i % w; row = i // w
    same = 2 <= col < w-2 and 2 <= row < h-2 and horizontal[i] and horizontal[i-w] and horizontal[i+w] and horizontal[i-2*w] and horizontal[i+2*w] and x[i-2*w] == x[i-w] == c == x[i+w] == x[i+2*w]
    flat += bool(same)
    if c == v: continue
    changed += 1; j = i*4
    delta = max(abs(a[j+k] - b[j+k]) for k in range(4)); all_max = max(all_max, delta); over += delta > 2
    if same: flat_max = max(flat_max, delta); bad += delta > 2
result = {'width': w, 'height': h, 'flat5x5Pixels': flat, 'flat5x5DeltaOver2': bad, 'flat5x5MaxDelta': flat_max, 'allDeltaOver2': over, 'allMaxDelta': all_max, 'changedFraction': changed / (w*h), 'scope': 'Corroborative CPU export versus actual GPU canvas after deselection without semantic mutation; flat-color5x5 check does not replace independent primitive-boundary AA oracles.'}
(root / 'comparison.json').write_text(json.dumps(result, indent=2) + '\n'); print(result)
assert bad == 0
