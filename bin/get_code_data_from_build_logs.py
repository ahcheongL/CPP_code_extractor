#!/usr/bin/env python3

import sys, os

import tqdm
import subprocess
import json
from utils import add_build_dir_to_path, remove_file

import tempfile

src_extensions = [".c", ".cpp", ".cc"]

subprocess_output = None  # Use None to inherit parent's stdio, or user subprocess.DEVNULL to suppress output


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

            if "conftest" in src_file:
                continue

            if "CMakeC" in src_file:
                # Skip CMake generated files
                continue
            if "TryCompile" in working_dir:
                # Skip CMake TryCompile directories
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
    result["src"] = dict()
    result["callgraph"] = dict()

    for command in tqdm.tqdm(commands):
        temp_output = tempfile.NamedTemporaryFile(delete=False)
        temp_output.close()
        temp_output_path = temp_output.name

        cmd = [
            "get_all_src",
            command.src_file,
            temp_output_path,
            "--",
        ] + command.command.split(" ")

        os.chdir(command.working_dir)

        process = subprocess.Popen(
            cmd,
            stdout=subprocess_output,
            stderr=subprocess_output,
            stdin=subprocess_output,
        )

        process.communicate()

        if not os.path.isfile(temp_output_path):
            print(f"Error: Output file {temp_output_path} not found.")
            continue

        try:
            with open(temp_output_path, "r") as f:
                src_defs = json.load(f)
        except json.JSONDecodeError:
            continue

        if src_defs is None:
            src_defs = dict()

        for file in src_defs:
            if file not in result["src"]:
                result["src"][file] = dict()

            for def_type in src_defs[file]:
                if def_type not in result["src"][file]:
                    result["src"][file][def_type] = dict()

                for name in src_defs[file][def_type].keys():
                    if isinstance(src_defs[file][def_type][name], list):
                        for definition in src_defs[file][def_type][name]:
                            result["src"][file][def_type][name] = []
                            if definition.get("code") is not None:
                                result["src"][file][def_type][name].append(definition)

                    elif isinstance(src_defs[file][def_type][name], dict):
                        if src_defs[file][def_type][name].get("code") is not None:
                            result["src"][file][def_type][name] = src_defs[file][
                                def_type
                            ][name]
                    else:
                        print(
                            f"Unknown definition type for {name} in {file} of type {def_type}"
                        )

        remove_file(temp_output_path)

        cmd = [
            "get_callgraph",
            command.src_file,
            temp_output_path,
            "--",
        ] + command.command.split(" ")

        process = subprocess.Popen(
            cmd,
            stdout=subprocess_output,
            stderr=subprocess_output,
            stdin=subprocess_output,
        )

        process.communicate()

        if not os.path.isfile(temp_output_path):
            print(f"Error: Output file {temp_output_path} not found.")
            continue

        try:
            with open(temp_output_path, "r") as f:
                callgraph = json.load(f)
        except json.JSONDecodeError:
            continue

        if callgraph is None:
            callgraph = dict()

        for func_name in callgraph:
            if func_name not in result["callgraph"]:
                result["callgraph"][func_name] = dict()
                # There can be multiple functions with the same name in different files
                # but let's keep it simple for now
                result["callgraph"][func_name]["callees"] = []
                result["callgraph"][func_name]["callers"] = []

            for callee in callgraph[func_name]["callees"]:
                if callee not in result["callgraph"][func_name]["callees"]:
                    result["callgraph"][func_name]["callees"].append(callee)
            for caller in callgraph[func_name]["callers"]:
                if caller not in result["callgraph"][func_name]["callers"]:
                    result["callgraph"][func_name]["callers"].append(caller)

        remove_file(temp_output_path)

    num_symbols = 0
    for file in result["src"]:
        for def_type in result["src"][file]:
            num_symbols += len(result["src"][file][def_type])

    print(f"Found {len(result['src'])} source files with {num_symbols} symbols.")

    with open(out_file_name, "w") as f:
        json.dump(result, f, indent=4)

    print(f"Output written to {out_file_name}")


if __name__ == "__main__":
    main(sys.argv)
