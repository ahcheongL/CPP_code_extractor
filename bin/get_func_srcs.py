#!/usr/bin/env python3

import sys, os
import tqdm
import subprocess
import json
import tempfile

from utils import add_build_dir_to_path, remove_file


def main(argv):
    if len(argv) < 2:
        print(
            f"Usage: {argv[0]} <src_list_file>\n"
            "<src_list_file> should contain a list of source files, one per line."
            "This script does not utilize the build configuration, "
            "use get_func_srcs_with_build_configuration.py instead."
        )

        return

    src_list_file = argv[1]

    try:
        with open(src_list_file, "r") as f:
            src_list = f.readlines()
    except FileNotFoundError:
        print(f"File {src_list_file} not found.")
        return

    src_list = [line.strip() for line in src_list]
    src_list = [line for line in src_list if line and not line.startswith("#")]

    add_build_dir_to_path()

    result = dict()

    for src_path in tqdm.tqdm(src_list):
        temp_output = tempfile.NamedTemporaryFile(delete=False)
        temp_output.close()
        temp_output_path = temp_output.name

        cmd = ["get_all_func_src", src_path, temp_output_path]

        process = subprocess.Popen(
            cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )

        process.communicate()

        if not os.path.isfile(temp_output_path):
            print(f"Error: Output file {temp_output_path} not found.")
            continue

        with open(temp_output_path, "r") as f:
            func_srcs = json.load(f)

        result[src_path] = func_srcs
        remove_file(temp_output_path)

    num_funcs = sum(len(v) for v in result.values())
    print(f"Found {len(result)} source files with {num_funcs} functions.")

    with open("out.json", "w") as f:
        json.dump(result, f, indent=4)

    print("Output written to out.json")

    return


if __name__ == "__main__":
    main(sys.argv)
