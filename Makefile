ifeq (, $(shell which llvm-config))
$(error "No llvm-config in $$PATH")
endif

LLVMVER  = $(shell llvm-config --version 2>/dev/null | sed 's/git//' | sed 's/svn//' )
LLVM_MAJOR = $(shell llvm-config --version 2>/dev/null | sed 's/\..*//' )
LLVM_MINOR = $(shell llvm-config --version 2>/dev/null | sed 's/.*\.//' | sed 's/git//' | sed 's/svn//' | sed 's/ .*//' )
$(info Detected LLVM VERSION : $(LLVMVER))


ifeq ($(origin CC), default)
	CC := clang
endif

ifeq ($(origin CXX), default)
	CXX := clang++
endif

AR := ar

LLVM_CXXFLAGS := `llvm-config --cxxflags` -fPIC -g -DLLVM_MAJOR=$(LLVM_MAJOR) -MMD -MP

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	LLVM_CXXFLAGS += -O0 -ggdb -DPRINT_DEBUG=1
else
	LLVM_CXXFLAGS += -O2
endif

CLANG_LIBS = -lclang-cpp

# add libLLVM.so if it exists
ifneq (, $(shell ldconfig -p | grep libLLVM.so))
	CLANG_LIBS += -lLLVM
endif

LLVM_LDFLAGS := `llvm-config --ldflags` $(CLANG_LIBS) -Wl,-rpath,$(shell llvm-config --libdir)

SRCS := $(wildcard src/*/*.cpp)
DEPS := $(patsubst src/%.cpp, build/%.d, $(SRCS))


.PHONY: all clean build_dir

all: build/get_func_list build/get_func_src build/get_all_src build/libextract.a build/get_callgraph build/parse_cpp build/get_diff

build/get_func_list: build/get_func_list.o build/cpp_code_extractor_util.o | build_dir
	$(CXX) -o $@ $^ $(LLVM_LDFLAGS)

build/parse_cpp: build/parse_cpp.o build/cpp_code_extractor_util.o | build_dir
	$(CXX) -o $@ $^ $(LLVM_LDFLAGS) -ljsoncpp

build/get_func_src: build/get_func_src.o build/cpp_code_extractor_util.o | build_dir
	$(CXX) -o $@ $^ $(LLVM_LDFLAGS) 

build/get_callgraph: build/get_callgraph.o build/cpp_code_extractor_util.o | build_dir
	$(CXX) -o $@ $^ $(LLVM_LDFLAGS) -ljsoncpp

build/get_all_src: build/get_all_src.o build/cpp_code_extractor_util.o | build_dir
	$(CXX) -o $@ $^ $(LLVM_LDFLAGS) -ljsoncpp

build/get_diff: build/get_diff.o build/cpp_code_extractor_util.o | build_dir
	$(CXX) -o $@ $^ $(LLVM_LDFLAGS) -ljsoncpp

build/%.o: src/%.cpp | build_dir
	$(CXX) $(LLVM_CXXFLAGS) -c -o $@ $^ -I include

build/libextract.a: build/cpp_code_extractor_util.o | build_dir
	$(AR) rcs $@ $^

build_dir:
	@mkdir -p build

clean:
	rm -rf build

-include $(DEPS)