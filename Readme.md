## Prerequisite

1. Ubuntu 22.04

2. Clang/LLVM 13.0.1+
    * It assumes `llvm-config, clang, clang++, ...` are on `PATH`.

3. `sudo apt install libjsoncpp-dev -y`

## Build
1. `make`

It will generate `get_func_list` and `get_func_src` in `build/`.

## Features

### 1. `get_func_list`

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


### 2. `get_func_src`

It will print the source code of function you want.

Usage:
```
./build/get_func_src <src_file_path> <func_name> -- [<compile args> ...]
```

Example:
```
./build/get_func_src ./src/get_func_list.cpp CreateASTConsumer -- -I include `llvm-config --cxxflags`
```

### 3. `cc_wrapper` and `cxx_wrapper`

cc/c++ wrapper so that it can extract compile command lines of source code.
It can be used to give more precise build configuration for other tools such as `get_func_list`.
It will write the extracted compile command lines to `/tmp/cl_output.txt`.
It just appends the new lines to the file, so make sure to remove it before running a build.
The output will contain build command lines with working directory;
The first word of each line is the working directory when the command line was invoked.

Usage:
Use cc_wrapper or cxx_wrapper when you build your program. It depends on your program's build mechanism.

Configurations (Environment variables):
1. `CL_OUTPUT`: output file path (default: `/tmp/cl_output.txt`)
2. `REAL_CC`, `REAL_CXX`: actual compiler path (default: `clang`/`clang++`)

Example:
```
rm /tmp/cl_output.txt; make clean; CC=cc_wrapper CXX=cxx_wrapper make
```


### 4. `get_all_func_src`

It extracts all function definitions in the given source code file and saves the code in a json file.

Usage:
```
./build/get_all_func_src <src_file_path> <output_json> -- [<compile args> ...]
```

Example:
```
./build/get_all_func_src ./src/get_func_list.cpp out.json -- -I include `llvm-config --cxxflags`
```

### 5. `get_func_srcs_with_build_configuration.py`

Input: A build configuration file (e.g., `/tmp/cl_output.txt`)
Output: A json file of function source code

Usage:
```
./bin/get_func_srcs_with_build_configuration.py <compile_commands.txt> <out.json>
```

### 6. `get_callgraph`

Usage:
```
./build/get_callgraph <src_file_path> <output_json> -- [<compile args> ...]
```

Example:
```
./build/get_callgraph ./src/get_func_list.cpp out.json -- -I include `llvm-config --cxxflags`
```