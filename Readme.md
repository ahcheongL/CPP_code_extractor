## Prerequisite

1. Ubuntu 22.04

2. Clang/LLVM 20.1.7+
    * It assumes `llvm-config, clang, clang++, ...` are on `PATH`.

3. `sudo apt install libjsoncpp-dev -y`

## Build
1. `make`

It will generate `gen_code_data` in `build/`.

## Features

### Compile command line extractor: `cc_wrapper`, `cxx_wrapper`

`cc_wrapper` and `cxx_wrapper` are cc/c++ wrappers so that they can extract compile command lines of source code.
They can be used to give more precise build configuration for other tools such as `gen_code_data`.
They will write the extracted compile command lines to `/tmp/cl_output.txt`.
They just append the new lines to the file, so make sure to remove it before running a build.
The output will contain build command lines with working directory;
The first word of each line is the working directory when the command line was invoked.

Usage:
Use `cc_wrapper` or `cxx_wrapper` when you build your program. It depends on your program's build mechanism.

Configurations (Environment variables):
1. `CL_OUTPUT`: output file path (default: `/tmp/cl_output.txt`)
2. `REAL_CC`, `REAL_CXX`: actual compiler path (default: `clang`/`clang++`)

Example:
```
rm /tmp/cl_output.txt; make clean; CC=cc_wrapper CXX=cxx_wrapper make
```

### Code data extractor: `gen_code_data`

It extracts all code data in the given source code file and saves the data in a json file.

Usage:
```
./build/gen_code_data <compile_commands.txt> <out.json>
```

It takes one environment variable: `EXCLUDES`: A space-separated list of path fragments to exclude from processing.

The json file structure is as follows:
```
{
  "<file_path>": {
    "functions": {
      "<func_name>": {
        "definition": "<source_code>",
        "start_line": <start_line>,
        "end_line": <end_line>,
        "calls": [ "<called_func_name1>", "<called_func_name2>", ... ],
        "callers": [ "<caller_func_name1>", "<caller_func_name2>", ... ],
        "variables": {
            "<var_name1>": {
                "definition": "<var_definition>",
                "start_line": <start_line>,
                "end_line": <end_line>
            },
            ...
        }
      },
      ...
    },
    "macros": {
        "<macro_name1>": {
            "definition": "<macro_definition>",
            "start_line": <start_line>,
            "end_line": <end_line>
        },
        ...
    },
    "enums": {
        "<enum_name1>": {
            "definition": "<enum_definition>",
            "start_line": <start_line>,
            "end_line": <end_line>
        }
        ...
    },
    "types": {
        "<type_name1>": {
            "definition": "<type_definition>",
            "start_line": <start_line>,
            "end_line": <end_line>
        }
        ...
    },
    "global_variables": {
        "<var_name1>": {
            "definition": "<var_definition>",
            "start_line": <start_line>,
            "end_line": <end_line>
        }
        ...
    },
    "disabled_macros": {
        "<macro_name1>": {
            "definition": "<macro_definition>",
            "start_line": <start_line>,
            "end_line": <end_line>
        }
        ...
    },
  },
}
```


### `get_func_list`

It will print the list of functions in the given source file.
Only functions that have a definition body in the given source file will be printed.

Usage:
```
./build/get_func_list <src_file_path> -- [<compile args> ...]
```

Example:
```
./build/get_func_list ./src/get_func_list.cpp -- -I include `llvm-config --cxxflags`
```


### `get_func_src`

It will print the source code of function you want.

Usage:
```
./build/get_func_src <src_file_path> <func_name> -- [<compile args> ...]
```

Example:
```
./build/get_func_src ./src/get_func_list.cpp CreateASTConsumer -- -I include `llvm-config --cxxflags`
```