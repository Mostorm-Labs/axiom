import json, os, pathlib, subprocess, sys, time

root = pathlib.Path.cwd()
out = root / 'out/fc02-resume/closeout-20261022'
out.mkdir(parents=True, exist_ok=True)
mode = sys.argv[1]
env = os.environ.copy()
facts = json.loads((root / 'out/fc02-resume/launch-3d7afa35/environment-native.json').read_text())
env.update(facts['environment'])
base = 'http://127.0.0.1:4175/index.html?rev=3d7afa35'
if mode == 'native':
    commands = [
        ['ctest', '--test-dir', str(root/'out/fc02-resume/build'), '--tests-from-file', str(root/'docs/planning/fc02/evidence/interface-repair-20261022/ctest-names-with-consumer.txt'), '--output-on-failure', '--output-junit', str(out/'ctest.xml'), '-j', '4'],
        [sys.executable, '-m', 'unittest', 'verification.tests.test_current_semantic_descriptor', 'verification.tests.test_semantic_contract', 'verification.tests.test_fc02_host_contract', 'verification.tests.test_host_text_font_resource', '-v'],
    ]
    names = ['ctest', 'python']
else:
    original = (root/'out/fc02-resume/reentry-web-existing.mjs').read_text()
    original = original.replace("path.resolve('out/fc02-host/chrome-isolated')", "path.resolve('out/fc02-resume/closeout-20261022/chrome-isolated')")
    original = original.replace('127.0.0.1:4174', '127.0.0.1:4175')
    original = original.replace('out/fc02-host/${name}.json', 'out/fc02-resume/closeout-20261022/${name}.json')
    (out/'web-existing.mjs').write_text(original)
    commands = [
        ['node', '--experimental-websocket', 'apps/axiom_canvas_demo/tests/fc02_text_consumer.mjs', base, str(out/'web-consumer.json')],
        ['node', '--experimental-websocket', str(out/'web-existing.mjs'), 'web_fc02_text_test', 'web_minimal_runtime_test'],
        ['node', '--experimental-websocket', 'out/fc02-resume/web-reentry-numeric-ime.mjs', base, str(out/'web-shape-reentry.json')],
    ]
    names = ['web-consumer', 'web-existing', 'web-shape-reentry']
record = {'source_revision': subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(), 'kind':mode, 'runs':[]}
for name, command in zip(names, commands):
    start = time.time()
    with (out/f'{name}.log').open('w', encoding='utf-8') as log:
        run = subprocess.run(command, cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT)
    record['runs'].append({'name':name,'command':command,'exit_code':run.returncode,'seconds':round(time.time()-start,3),'log':f'{name}.log'})
    (out/f'{mode}-commands.json').write_text(json.dumps(record,indent=2))
    print(name, 'exit', run.returncode, flush=True)
    print((out/f'{name}.log').read_text(errors='replace')[-1400:],flush=True)
    if run.returncode:
        sys.exit(run.returncode)
