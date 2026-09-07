"""Pure target arguments plus explicit producer toolchain identification."""
from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess

from tools.sdk.model import SdkError
from tools.semantic.contract import RUNTIME_KEYS, validate_profile


@dataclass(frozen=True)
class ToolchainSelection:
    cmake_args: tuple[str, ...]
    record: dict


def target_cmake_arguments(profile: dict, key: str, *, ndk: Path | None = None,
                           emscripten: Path | None = None, cc: str | None = None,
                           cxx: str | None = None) -> list[str]:
    validate_profile(profile)
    if key not in RUNTIME_KEYS:
        raise SdkError(f"semantic: unsupported runtime target: {key}")
    target = profile["runtimes"][key]
    family = target["platform"]
    args = ["-DCMAKE_POSITION_INDEPENDENT_CODE=ON", "-DBUILD_SHARED_LIBS=OFF"]
    if family in {"linux", "windows", "macos"}:
        if cc is not None:
            args.append(f"-DCMAKE_C_COMPILER={cc}")
        if cxx is not None:
            args.append(f"-DCMAKE_CXX_COMPILER={cxx}")
    if family == "windows":
        args += ["-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded", "-DABSL_MSVC_STATIC_RUNTIME=ON",
                 "-DCMAKE_CXX_FLAGS=/D_ITERATOR_DEBUG_LEVEL=0 /D_HAS_ITERATOR_DEBUGGING=0 /Brepro"]
    elif family in {"macos", "ios", "ios-simulator"}:
        args += [f"-DCMAKE_OSX_ARCHITECTURES={target['arch']}",
                 f"-DCMAKE_OSX_SYSROOT={target['toolchain']['sdk']}"]
        if family != "macos":
            args += ["-DCMAKE_SYSTEM_NAME=iOS", "-DCMAKE_OSX_DEPLOYMENT_TARGET=" + target["toolchain"]["deploymentTarget"],
                     "-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO"]
    elif family == "android":
        if ndk is None:
            raise SdkError("semantic: Android requires an explicit NDK root")
        args += [f"-DCMAKE_TOOLCHAIN_FILE={ndk.resolve().as_posix()}/build/cmake/android.toolchain.cmake",
                 f"-DANDROID_ABI={target['arch']}", f"-DANDROID_PLATFORM=android-{target['toolchain']['apiLevel']}",
                 "-DANDROID_STL=c++_static"]
    elif family == "web":
        if emscripten is None:
            raise SdkError("semantic: Web requires an explicit Emscripten root")
        args += [f"-DCMAKE_TOOLCHAIN_FILE={emscripten.resolve().as_posix()}/cmake/Modules/Platform/Emscripten.cmake",
                 "-DCMAKE_CXX_FLAGS=-fexceptions", "-DCMAKE_EXE_LINKER_FLAGS=-fexceptions"]
    return args


def _output(command: list[str]) -> str:
    result = subprocess.run(command, check=True, text=True, capture_output=True, input="")
    return (result.stdout + result.stderr).strip()


def _find(name: str) -> str:
    result = shutil.which(name)
    if result is None:
        raise SdkError(f"semantic: required tool is not installed: {name}")
    return result


def identify_toolchain(profile: dict, key: str, *, ndk: Path | None = None,
                       emscripten: Path | None = None, cc: str | None = None,
                       cxx: str | None = None) -> ToolchainSelection:
    target = profile["runtimes"][key]
    family = target["platform"]
    extra: dict = {}
    compiler_args: list[str] = []
    if family == "linux":
        cc, cxx = cc or _find("gcc"), cxx or _find("g++")
        compiler = "gcc"
        extra["glibcVersion"] = " ".join(platform.libc_ver())
    elif family == "windows":
        cc, cxx = cc or _find("clang-cl"), cxx or _find("clang-cl")
        compiler = "clang-cl"
        extra["llvmVersion"] = target["toolchain"]["llvm"]
        for env_name, field in (("VCToolsVersion", "msvcVersion"), ("WindowsSDKVersion", "windowsSdkVersion")):
            value = os.environ.get(env_name, "").rstrip("\\/")
            if not value:
                raise SdkError(f"semantic: activate the MSVC developer environment ({env_name})")
            extra[field] = value
    elif family in {"macos", "ios", "ios-simulator"}:
        sdk = target["toolchain"]["sdk"]
        cc = cc or _output(["xcrun", "--sdk", sdk, "--find", "clang"])
        cxx = cxx or _output(["xcrun", "--sdk", sdk, "--find", "clang++"])
        compiler = "apple-clang"
        extra.update(sdkName=sdk, sdkVersion=_output(["xcrun", "--sdk", sdk, "--show-sdk-version"]),
                     xcodeVersion=_output(["xcodebuild", "-version"]))
        deployment = target["toolchain"].get("deploymentTarget")
        if family == "macos":
            deployment = os.environ.get("MACOSX_DEPLOYMENT_TARGET")
            if not deployment:
                sysroot = _output(["xcrun", "--sdk", sdk, "--show-sdk-path"])
                details = _output([cxx, "-###", "-x", "c++", "-c", "-", "-arch", target["arch"], "-isysroot", sysroot])
                found = re.search(r"apple-macosx([0-9]+(?:\.[0-9]+)*)", details)
                if found is None:
                    raise SdkError("semantic: cannot identify the compiler's default macOS deployment target")
                deployment = found.group(1)
        extra["deploymentTarget"] = deployment
    elif family == "android":
        if ndk is None:
            raise SdkError("semantic: Android NDK root is required")
        properties = (ndk / "source.properties").read_text(encoding="utf-8")
        expected = target["toolchain"]["ndk"]
        if not re.search(r"Pkg.Revision\s*=\s*" + re.escape(expected) + r"(?:\s|$)", properties):
            raise SdkError("semantic: Android NDK version differs from the profile")
        host = {"Linux": "linux-x86_64", "Darwin": "darwin-x86_64", "Windows": "windows-x86_64"}[platform.system()]
        suffix = ".exe" if os.name == "nt" else ""
        cxx = str(ndk / "toolchains/llvm/prebuilt" / host / "bin" / ("clang++" + suffix))
        compiler = "clang"
        triple = "aarch64-linux-android" if target["arch"] == "arm64-v8a" else "x86_64-linux-android"
        compiler_args = [f"--target={triple}{target['toolchain']['apiLevel']}"]
        extra.update(ndkVersion=expected, apiLevel=target["toolchain"]["apiLevel"])
    elif family == "web":
        if emscripten is None:
            raise SdkError("semantic: Emscripten root is required")
        cxx = str(emscripten / ("em++.bat" if os.name == "nt" else "em++"))
        compiler = "emscripten"
        extra.update(emscriptenVersion=target["toolchain"]["emscripten"], llvmVersion=target["toolchain"]["llvm"])
    else:
        raise SdkError(f"semantic: unsupported producer platform: {family}")
    version = _output([cxx, *compiler_args, "--version"])
    if family == "windows" and not re.search(r"\b" + re.escape(extra["llvmVersion"]) + r"\b", version):
        raise SdkError("semantic: Windows LLVM version differs from the profile")
    if family == "web" and not re.search(r"\b" + re.escape(extra["emscriptenVersion"]) + r"\b", version):
        raise SdkError("semantic: Emscripten version differs from the profile")
    match = re.search(r"Target:\s*(\S+)", version)
    triple = match.group(1) if match else _output([cxx, *compiler_args, "-dumpmachine"])
    record = {"compiler": compiler, "compilerVersion": version.splitlines()[0], "compilerTarget": triple,
              "cmakeVersion": _output(["cmake", "--version"]).splitlines()[0],
              "ninjaVersion": _output(["ninja", "--version"]).splitlines()[0],
              "cxxRuntime": target["abi"]["cxxRuntime"], **extra}
    args = target_cmake_arguments(profile, key, ndk=ndk, emscripten=emscripten, cc=cc, cxx=cxx)
    if family == "macos":
        args.append("-DCMAKE_OSX_DEPLOYMENT_TARGET=" + record["deploymentTarget"])
    return ToolchainSelection(tuple(args), record)
