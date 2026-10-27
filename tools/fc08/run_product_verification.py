"""Scoped real-SUT FC08 bootstrap runner. Missing coverage is explicit, never PASS."""
import argparse
from pathlib import Path
from product_runner import execute

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--design',type=Path,required=True);p.add_argument('--case',required=True)
    p.add_argument('--profile',choices=['windows-native','web-wasm','headless'],required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--native-build',type=Path,required=True);p.add_argument('--web-build',type=Path,required=True)
    p.add_argument('--protoc',type=Path,required=True);p.add_argument('--node',type=Path,required=True);p.add_argument('--chrome',type=Path,required=True)
    raise SystemExit(execute(p.parse_args()))

if __name__=='__main__':main()
