"""Shared CMake package search arguments for relocated Semantic SDK consumers."""
from pathlib import Path


def consumer_cmake_arguments(runtime_root: Path, protoc: Path) -> list[str]:
    # CMake normalizes search prefixes but does not normalize every re-rooting
    # input identically. Windows 8.3 aliases (for example RUNNER~1) can otherwise
    # leave no searchable prefix under MODE_PACKAGE=ONLY. Preserve the strict
    # search boundary and supply one real spelling for every SDK path.
    runtime_root = runtime_root.resolve(strict=True)
    protoc = protoc.resolve(strict=True)
    return [
        f"-DCMAKE_PREFIX_PATH={runtime_root.as_posix()}",
        f"-DAXIOM_PROTOC={protoc.as_posix()}",
        f"-DCMAKE_FIND_ROOT_PATH={runtime_root.as_posix()}",
        "-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY",
        "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
        "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF",
    ]
