#! /usr/bin/env python3
import os
import subprocess
import sys

from typing import Set


def main() -> int:
    ldargs = sys.argv[1:]
    output = subprocess.run([*ldargs, "--verbose"], capture_output=True)
    lines = output.stdout.decode('utf-8').splitlines()
    libs: Set[str] = set()
    for line in lines:
        if not line.endswith(".a"):
            continue
        if os.path.isfile(line):
            libs.add(os.path.abspath(line))

    print(os.linesep.join(libs))
    return 0


if __name__ == "__main__":
    sys.exit(main())
