#ifndef CPP_CODE_EXTRACTOR_UTIL_HPP
#define CPP_CODE_EXTRACTOR_UTIL_HPP

#include <string>
#include <vector>

std::vector<std::string> get_compile_args(int argc, const char **argv);
bool                     is_system_file(const std::string &file_path);
std::string              get_abs_path(const std::string &file_path);
std::string              strip(const std::string &str);
bool ends_with(const std::string &str, const std::string &suffix);

#endif