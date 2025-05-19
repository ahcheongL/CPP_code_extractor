## Prerequisite

1. Ubuntu 22.04

2. Clang/LLVM 13.0.1+
    * It assumes `llvm-config, clang, clang++, ...` are on `PATH`.

## Build
1. `make`

It will generate `get_func_list` and `get_func_src` in `build/`.

## Features

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