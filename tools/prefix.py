#! /usr/bin/env python3
import argparse
import os
import os.path
import sys


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Utility for prefixing library compiles with a certain prefix."
    )
    parser.add_argument(
        'prefix',
        metavar='PREFIX',
        type=str,
        help='The prefix we want to ensure is on the file.',
    )
    parser.add_argument(
        'file',
        metavar='FILE',
        type=str,
        help='The file path (could be absolute or relative) that needs the file prefixed.',
    )
    parser.add_argument(
        '--strip-dir',
        action="store_true",
        help='Whether we should strip any leading directory when printing out final concatenation.',
    )
    args = parser.parse_args()

    if os.path.sep in args.file:
        preamble, filename = args.file.rsplit(os.path.sep, 1)
        if filename[:len(args.prefix)] != args.prefix:
            filename = args.prefix + filename
        
        if args.strip_dir:
            print(f"{filename}")
        else:
            print(f"{preamble}{os.path.sep}{filename}")
    else:
        if args.file[:len(args.prefix)] != args.prefix:
            print(f"{args.prefix}{args.file}")
        else:
            print(f"{args.file}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
