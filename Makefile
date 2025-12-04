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

LLVM_LDFLAGS := `llvm-config --ldflags` $(CLANG_LIBS) -Wl,-rpath,$(shell llvm-config --libdir)

SRCS := $(wildcard src/*/*.cpp)
DEPS := $(patsubst src/%.cpp, build/%.d, $(SRCS))


.PHONY: all clean build_dir

all: build/get_func_list build/get_func_src build/libextract.a build/gen_code_data

build/get_func_list: build/get_func_list.o build/cpp_code_extractor_util.o | build_dir
	$(CXX) -o $@ $^ $(LLVM_LDFLAGS)

build/get_func_src: build/get_func_src.o build/cpp_code_extractor_util.o | build_dir
	$(CXX) -o $@ $^ $(LLVM_LDFLAGS) 

build/%.o: src/%.cpp | build_dir
	$(CXX) $(LLVM_CXXFLAGS) -c -o $@ $^ -I include

build/libextract.a: build/cpp_code_extractor_util.o | build_dir
	$(AR) rcs $@ $^

build/gen_code_data: build/gen_code_data.o build/cpp_code_extractor_util.o build/json_utils.o | build_dir
	$(CXX) -o $@ $^ $(LLVM_LDFLAGS) -ljsoncpp

build_dir:
	@mkdir -p build

clean:
	rm -rf build

-include $(DEPS)