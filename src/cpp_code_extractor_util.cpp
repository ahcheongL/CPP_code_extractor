#include "cpp_code_extractor_util.hpp"

#include <limits.h>
#include <string.h>

#include <iostream>

// Assume argv contains "--" followed by compile arguments
// If it does not, return an empty std::vector
std::vector<std::string> get_compile_args(int argc, const char **argv) {
  std::vector<std::string> result;

  unsigned int idx = 0;

  while (idx < argc && argv[idx] != nullptr) {
    if (strcmp(argv[idx], "--") == 0) {
      idx++;
      break;
    }
    idx++;
  }

  while (idx < argc && argv[idx] != nullptr) {
    result.push_back(argv[idx]);
    idx++;
  }

  add_system_include_paths(result);

  return result;
}

static const std::vector<std::string> &get_system_include_dirs() {
  static std::vector<std::string> system_include_dirs;

  if (!system_include_dirs.empty()) { return system_include_dirs; }

  const char *cmd = "clang -E -x c++ - -v < /dev/null 2>&1";
  FILE       *fp = popen(cmd, "r");
  if (fp == NULL) {
    std::cerr << "Error: could not run command: " << cmd << "\n";
    return system_include_dirs;
  }

  char buffer[256];
  bool found_include_start = false;
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    std::string line(buffer);
    if (found_include_start) {
      if (line.find("End of search list.") != std::string::npos) {
        found_include_start = false;
        continue;
      }

      if (line.find("starts here") != std::string::npos) { continue; }

      if (line.empty()) { continue; }

      if (line[0] == ' ') { line = line.substr(1); }

      const size_t len = line.length();

      if (line[len - 1] == '\n') { line = line.substr(0, len - 1); }

      system_include_dirs.push_back(line);
      continue;
    }

    if (line.find("#include <...> search starts here") != std::string::npos) {
      found_include_start = true;
      continue;
    }

    if (line.find("#include \"...\" search starts here") != std::string::npos) {
      found_include_start = true;
      continue;
    }
  }
  pclose(fp);
  return system_include_dirs;
}

// Execute clang and get the system include paths
// and add them to the compile args
void add_system_include_paths(std::vector<std::string> &compile_args) {
  const std::vector<std::string> &system_include_dirs =
      get_system_include_dirs();

  for (const std::string &dir : system_include_dirs) {
    compile_args.push_back("-isystem");
    compile_args.push_back(dir);
  }

  return;
}

bool is_system_file(const std::string &file_path) {
  const std::vector<std::string> &system_include_dirs =
      get_system_include_dirs();

  for (const std::string &dir : system_include_dirs) {
    if (file_path.find(dir) == 0) { return true; }
  }

  return false;
}

std::string get_canonical_abs_path(const std::string &file_path) {
  if (file_path == "") { return ""; }
  char abs_path[PATH_MAX];
  if (realpath(file_path.c_str(), abs_path) == nullptr) {
    std::cerr << "Error: could not get absolute path for " << file_path << "\n";
    return "";
  }
  return std::string(abs_path);
}

std::string strip(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\n\r\f\v");  // all whitespace
  if (start == std::string::npos) return "";  // std::string is all whitespace
  size_t end = s.find_last_not_of(" \t\n\r\f\v");
  return s.substr(start, end - start + 1);
}

bool ends_with(const std::string &s, const std::string &suffix) {
  const size_t str_size = s.size();
  const size_t suffix_size = suffix.size();
  if (str_size < suffix_size) return false;
  return s.compare(str_size - suffix_size, suffix_size, suffix) == 0;
}

std::vector<std::string> tokenize_command(const std::string &command) {
  std::vector<std::string> tokens;
  size_t                   pos = 0;
  size_t                   space_pos = command.find(' ');

  while (space_pos != std::string::npos) {
    std::string token = command.substr(pos, space_pos - pos);
    tokens.push_back(token);
    pos = space_pos + 1;
    space_pos = command.find(' ', pos);
  }

  // last token
  if (pos < command.length()) {
    std::string token = command.substr(pos);
    tokens.push_back(token);
  }

  return tokens;
}