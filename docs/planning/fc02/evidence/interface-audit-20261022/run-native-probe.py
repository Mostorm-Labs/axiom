"""Build/run the FC-02 public RuntimeFacade consumer probe on Windows/MSVC."""
import ctypes
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[5]
build = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / "out/fc02-resume/build"
font = pathlib.Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else root / "assets/fonts/noto-sans-cjk-sc/NotoSansCJKsc-Regular.otf"
out = root / "out/fc02-resume/interface-audit"
out.mkdir(parents=True, exist_ok=True)

vs = r"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
raw = subprocess.check_output(f'call "{vs}" -arch=x64 >nul && set', shell=True, text=True)
env = {k.upper(): v for k, v in os.environ.items()}
for line in raw.splitlines():
    if "=" in line and not line.startswith("="):
        key, value = line.split("=", 1)
        env[key.upper()] = value

def split_windows(value):
    count = ctypes.c_int()
    command_line_to_argv = ctypes.windll.shell32.CommandLineToArgvW
    command_line_to_argv.restype = ctypes.POINTER(ctypes.c_wchar_p)
    ptr = command_line_to_argv(value, ctypes.byref(count))
    try:
        return [ptr[i] for i in range(count.value)]
    finally:
        ctypes.windll.kernel32.LocalFree(ptr)

commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
entry = next(row for row in commands if row["file"].endswith("windows_text_pipeline_test.cpp"))
args = split_windows(entry["command"])
args = [arg for arg in args if not arg.startswith(("/Fo", "/Fd"))
        and not arg.lower().endswith(".cpp") and arg not in {"-c", "/FS", "/DNDEBUG", "-DAXIOM_RUNTIME_QUALIFICATION=1"}]
args += [str(pathlib.Path(__file__).with_name("probe-native.cpp")), "/Fo" + str(out / "probe-native.obj")]
ninja = (build / "build.ninja").read_text(encoding="utf-8")
block = re.search(r"build apps\\minimal_runtime_host\\axiom_minimal_windows_text_pipeline_tests\.exe:.*?(?=\n\n)", ninja, re.S).group()
libraries = re.search(r"  LINK_LIBRARIES = (.*)", block).group(1)
args += ["/link", "/INCREMENTAL:NO", "/SUBSYSTEM:CONSOLE", "/OUT:" + str(out / "probe-native.exe")]
args += split_windows(libraries)
(out / "native-command.json").write_text(json.dumps({"cwd": str(build), "args": args,
    "VCToolsVersion": env.get("VCTOOLSVERSION"), "qualification_api_enabled": False}, indent=2), encoding="utf-8")
rsp = out / "probe-native.rsp"
rsp.write_text(subprocess.list2cmdline(args[1:]), encoding="utf-8")
compiled = subprocess.run([args[0], "@" + str(rsp)], cwd=build, env=env, capture_output=True, text=True, errors="replace")
(out / "native-build.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
print((compiled.stdout + compiled.stderr)[-3000:])
compiled.check_returncode()
shutil.copyfile(build / "apps/minimal_runtime_host/icudtl.dat", out / "icudtl.dat")
ran = subprocess.run([str(out / "probe-native.exe"), str(font)], cwd=build / "apps/minimal_runtime_host", env=env, capture_output=True, text=True, errors="replace")
(out / "native-result.json").write_text(ran.stdout, encoding="utf-8")
print(ran.stdout, ran.stderr)
ran.check_returncode()
values = json.loads(ran.stdout)
checks = {
    "minimal_facade_font_readback_gap_reproduced": values["direct_catalog"] == 1 and values["wrapped_catalog"] == 0
        and values["direct_resource_status"] and not values["wrapped_resource_status"],
    "runtime_preedit_caret_supported": values["preedit_geometry_available"] and values["preedit_caret_one"] < values["preedit_caret_three"],
    "runtime_format_supported": values["format_status"] in {2, 3} and values["format_bold"],
    "runtime_zero_width_rejected": values["zero_width_status"] == 4,
}
(out / "native-checks.json").write_text(json.dumps(checks, indent=2), encoding="utf-8")
if not all(checks.values()):
    raise SystemExit("Probe outcome changed: inspect native-result.json before relying on old findings")
print("EXPECTED_GAPS_REPRODUCED; no product acceptance claim")
