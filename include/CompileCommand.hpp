#ifndef COMPILE_COMMAND_HPP
#define COMPILE_COMMAND_HPP

#include <string>
#include <vector>
class CompileCommand {
 public:
  CompileCommand();
  CompileCommand(const std::string              &working_dir,
                 const std::vector<std::string> &command,
                 const std::string              &src_file)
      : working_dir_(working_dir), command_(command), src_file_(src_file) {
  }

  std::string              working_dir_ = "";
  std::vector<std::string> command_ = {};
  std::string              src_file_ = "";
};

#endif