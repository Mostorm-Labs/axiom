"""Shared CMake package search arguments for relocated Semantic SDK consumers."""
from pathlib import Path


def consumer_cmake_arguments(runtime_root: Path, protoc: Path) -> list[str]:
    return [
        f"-DCMAKE_PREFIX_PATH={runtime_root.as_posix()}",
        f"-DAXIOM_PROTOC={protoc.as_posix()}",
        f"-DCMAKE_FIND_ROOT_PATH={runtime_root.as_posix()}",
        "-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY",
        "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
        "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF",
    ]
