#ifndef CPP_CODE_EXTRACTOR_UTIL_HPP
#define CPP_CODE_EXTRACTOR_UTIL_HPP

#include <string>
#include <vector>

using namespace std;

vector<string> get_compile_args(int argc, const char **argv);
bool           is_system_file(const string &file_path);
#endif