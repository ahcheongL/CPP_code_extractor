#!/usr/bin/env python3

import sys, os

import tqdm
import subprocess
import json
from utils import add_build_dir_to_path, remove_file

import tempfile

src_extensions = [".c", ".cpp", ".cc"]


class CompileCommand:
    def __init__(self, working_dir, command, src_file):
        self.working_dir = working_dir
        self.command = command
        self.src_file = src_file

    def __repr__(self):
        return f"CompileCommand({self.working_dir}, {self.command}, {self.src_file})"


def read_compile_commands(file_name):
    commands = []
    with open(file_name, "r") as f:
        for line in f:
            line = line.strip()

            working_dir = line.split(" ")[0]
            command = line.split(" ")[1:]

            if "-c" not in command:
                # Skip commands that do not contain the -c flag
                continue

            src_file_idx = -1
            for i, arg in enumerate(command):
                if any(arg.endswith(ext) for ext in src_extensions):
                    src_file_idx = i
                    break

            if src_file_idx == -1:
                print(f"Source file not found in command: {line}")
                continue

            src_file = command[src_file_idx]

            if src_file == "conftest.c":
                continue

            if "CMakeC" in src_file:
                # Skip CMake generated files
                continue

            command = command[:src_file_idx] + command[src_file_idx + 1 :]
            command = " ".join(command)

            if not os.path.isabs(src_file):
                src_file = os.path.join(working_dir, src_file)

            commands.append(CompileCommand(working_dir, command, src_file))

    return commands


def main(argv):
    if len(argv) != 3:
        print(f"Usage: {argv[0]} <compile_commands.txt> <out.json>")
        print(f"<compile_commands.txt> should be output of cc_wrapper and cxx_wrapper.")
        return

    compile_commands_file_name = argv[1]
    if not os.path.isfile(compile_commands_file_name):
        print(f"File {compile_commands_file_name} does not exist.")
        return

    out_file_name = argv[2]
    if not os.path.isabs(out_file_name):
        out_file_name = os.path.join(os.getcwd(), out_file_name)

    commands = read_compile_commands(compile_commands_file_name)

    add_build_dir_to_path()

    result = dict()

    for command in tqdm.tqdm(commands):
        temp_output = tempfile.NamedTemporaryFile(delete=False)
        temp_output.close()
        temp_output_path = temp_output.name

        cmd = [
            "get_all_func_src",
            command.src_file,
            temp_output_path,
            "--",
        ] + command.command.split(" ")

        os.chdir(command.working_dir)

        process = subprocess.Popen(
            cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            stdin=subprocess.DEVNULL,
        )

        process.communicate()

        if not os.path.isfile(temp_output_path):
            print(f"Error: Output file {temp_output_path} not found.")
            continue

        with open(temp_output_path, "r") as f:
            func_srcs = json.load(f)

        if func_srcs is None:
            func_srcs = dict()

        result[command.src_file] = func_srcs
        remove_file(temp_output_path)

    num_funcs = sum(len(v) for v in result.values())
    print(f"Found {len(result)} source files with {num_funcs} functions.")

    with open(out_file_name, "w") as f:
        json.dump(result, f, indent=4)

    print(f"Output written to {out_file_name}")


if __name__ == "__main__":
    main(sys.argv)
