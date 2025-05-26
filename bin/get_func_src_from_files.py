#!/usr/bin/env python3

import sys, os
import tqdm
import subprocess
import json


def main(argv):
    if len(argv) < 2:
        print(f"Usage: {argv[0]} <src_list_file> [<compile_args> ...]")
        return

    src_list_file = argv[1]

    compile_args = argv[2:]

    try:
        with open(src_list_file, "r") as f:
            src_list = f.readlines()
    except FileNotFoundError:
        print(f"File {src_list_file} not found.")
        return

    src_list = [line.strip() for line in src_list]
    src_list = [line for line in src_list if line and not line.startswith("#")]

    build_dir = os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build"
    )

    env = os.environ.copy()
    env["PATH"] = env["PATH"] + ":" + build_dir

    result = dict()

    for src_path in tqdm.tqdm(src_list):
        cmd = ["get_func_list", src_path, "--"] + compile_args

        process = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env
        )

        stdout, _ = process.communicate()

        try:
            stdout = stdout.decode("utf-8").strip()
        except UnicodeDecodeError:
            continue

        func_list = []
        for line in stdout.split("\n"):
            if line == "":
                continue
            func_list.append(line)

        if len(func_list) == 0:
            continue

        result[src_path] = {}

        # print("Found %d functions in %s" % (len(func_list), src_path))
        # for i in range(len(func_list)):
        #     print("\t%d: %s" % (i, func_list[i]))

        for func in func_list:
            cmd = ["get_func_src", src_path, func, "--"] + compile_args

            process = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env
            )

            stdout, _ = process.communicate()
            try:
                stdout = stdout.decode("utf-8").strip()
            except UnicodeDecodeError:
                continue

            if len(stdout) == 0:
                continue

            result[src_path][func] = stdout

    num_funcs = sum(len(v) for v in result.values())
    print(f"Found {len(result)} source files with {num_funcs} functions.")

    with open("out.json", "w") as f:
        json.dump(result, f, indent=4)

    print("Output written to out.json")

    return


if __name__ == "__main__":
    main(sys.argv)
