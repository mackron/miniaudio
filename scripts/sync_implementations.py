#!/usr/bin/env python3

from typing import List

import argparse
import os
import sys
import pathlib
import re


section_begin_regex = re.compile(
    r"^\s*/\*\s*SECTION:?\s*((?:\w|\.)+)\s*\*/$", re.IGNORECASE | re.MULTILINE)
section_end_regex = re.compile(
    r"^\s*/\*\s*END\s*SECTION:?\s*((?:\w|\.)+)\s*\*/$", re.IGNORECASE | re.MULTILINE)


class Section:
    def __init__(self, filename: str, begin: int, end: int):
        self.filename = filename
        self.begin = begin
        self.end = end


def get_last_modified(path: str) -> float:
    """Gets a timestamp indicating when the given path was last modified. If the given path
    points to a file, then the returned timestamp is its last modified date and if the path
    points to a directory, the returned timestamp will be the most recent last modified date
    of all files inside this directory (and its subdirectories)"""

    if os.path.isfile(path):
        return os.stat(path).st_mtime
    else:
        assert os.path.isdir(path)

        most_recent_timestamp = 0

        for current_file in [f for f in pathlib.Path(path).glob("**/*") if f.is_file()]:
            most_recent_timestamp = max(
                most_recent_timestamp, os.stat(current_file).st_mtime)

        return most_recent_timestamp


def parse_unified_header(path: str) -> List[Section]:
    """Parses the unified header and extracts the marked-up sections in it"""

    with open(path, "r") as header_file:
        content = header_file.read()

        found_begins = re.finditer(section_begin_regex, content)
        found_ends = re.finditer(section_end_regex, content)

        sections = []

        for begin_match, end_match in zip(found_begins, found_ends):
            filename = begin_match.group(1)

            assert end_match.group(
                1) == filename, "Mismatched begin and end markers for section"
            assert begin_match.start() + 1 <= end_match.end() - 1

            sections.append(Section(
                filename=filename, begin=begin_match.end() + 1, end=end_match.start() - 1))

        return sections


def split_to_unified(unified_header_file: str, split_implementation_dir: str):
    """Synchronizes the changes made in the split implementation to be reflected in
    the unified, single-header implementation"""

    sections = parse_unified_header(unified_header_file)

    files = [f.name for f in pathlib.Path(
        split_implementation_dir).glob("**/*") if f.is_file()]

    content = open(unified_header_file, "r").read()

    print(files)

    for current_section in reversed(sections):
        if not current_section.filename in files:
            print("Deleting files is not yet implemented", file=sys.stderr)
            sys.exit(1)

        # Replace this section in the unified header
        content = "".join([content[: current_section.begin], open(os.path.join(
            split_implementation_dir, current_section.filename)).read(), content[current_section.end:]])

        # remove the processed file
        files.remove(current_section.filename)

    if not len(files) == 0:
        print("Adding new files not yet implemented", file=sys.stderr)
        sys.exit(1)

    with open(unified_header_file, "w") as out_file:
        out_file.write(content)


def unified_to_split(unified_header_file: str, split_implementation_dir: str):
    """Synchronizes the changes made in the unified, single-header implementation to
    be reflected in the split implementation"""

    sections = parse_unified_header(unified_header_file)

    content = open(unified_header_file, "r").read()

    for current_section in sections:
        print(current_section.filename)
        with open(os.path.join(split_implementation_dir, current_section.filename), "w") as out_file:
            out_file.write(content[current_section.begin: current_section.end])


def main():
    source_root = os.path.normpath(
        os.path.join(os.path.dirname(__file__), ".."))

    parser = argparse.ArgumentParser(
        description="Synchronizes the single-header and the split implementation")
    parser.add_argument("--single-header", help="Path to the single-header implementation", metavar="PATH",
                        default=os.path.join(source_root, "miniaudio.h"))
    parser.add_argument("--split-implementation", help="Path to the directory in which the split implementation lives", metavar="PATH",
                        default=os.path.join(source_root, "extras", "miniaudio_split"))

    args = parser.parse_args()

    if not os.path.isfile(args.single_header):
        print("[ERROR]: \"%s\" is not a file" %
              args.single_header, file=sys.stderr)
        sys.exit(1)
    if not os.path.isdir(args.split_implementation):
        print("[ERROR]: \"%s\" is not a directory" %
              args.split_implementation, file=sys.stderr)
        sys.exit(1)

    unified_timestamp = get_last_modified(args.single_header)
    split_timestamp = get_last_modified(args.split_implementation)

    if unified_timestamp < split_timestamp:
        print("Synchronizing split -> unified")
        split_to_unified(args.single_header, args.split_implementation)
    else:
        print("Synchronizing unified -> split")
        unified_to_split(args.single_header, args.split_implementation)


if __name__ == "__main__":
    main()
