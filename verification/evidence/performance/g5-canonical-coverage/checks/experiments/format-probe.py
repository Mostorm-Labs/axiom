import shlex,subprocess
from pathlib import Path
r=subprocess.check_output(['ninja','-C','out/host','-t','commands','runtime/render/axiom_prepared_geometry_tests'],text=True)
line=[s for s in r.splitlines() if ' -o runtime/render/axiom_prepared_geometry_tests ' in s][-1]
line=line.removeprefix(': && ').removesuffix(' && :');args=shlex.split(line)
i=args.index('-o');args[i+1]=str(Path('out/g5-canonical-format-probe').resolve())
obj=next(j for j,v in enumerate(args) if v.endswith('prepared_geometry_test.cpp.o'))
args[obj]=str(Path('out/g5-canonical-format-probe.o').resolve())
cmd=[args[0],'-std=c++23','-O2','-I/Users/qing/Desktop/sources/git/deps/axiom/darwin-arm64/skia-sdk/macos-arm64-metal/release','-c','out/g5-canonical-format-probe.cpp','-o','out/g5-canonical-format-probe.o']
subprocess.run(cmd,check=True);subprocess.run(args,cwd='out/host',check=True);subprocess.run(['out/g5-canonical-format-probe'],check=True)
