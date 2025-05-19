#include "get_func_list.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"

///////////////////////
// FunctionVisitor class
////////////////////////

FunctionVisitor::FunctionVisitor(clang::SourceManager &src_manager,
                                 const char           *src_path)
    : src_manager_(src_manager), src_path_(src_path) {
}

bool FunctionVisitor::VisitFunctionDecl(clang::FunctionDecl *FuncDecl) {
  if (!FuncDecl->isThisDeclarationADefinition()) { return true; }

  string func_name = FuncDecl->getNameInfo().getName().getAsString();
  clang::SourceLocation loc = FuncDecl->getLocation();

  // get filename
  const clang::FileEntry *file_entry =
      src_manager_.getFileEntryForID(src_manager_.getFileID(loc));
  if (file_entry == nullptr) { return true; }

  const char *file_name = file_entry->getName().data();
  if (strcmp(file_name, src_path_) != 0) { return true; }

  std::cerr << func_name << "\n";

  return true;
}

////////////////////////
// FunctionASTConsumer class
////////////////////////

FunctionASTConsumer::FunctionASTConsumer(clang::SourceManager &src_manager,
                                         const char           *src_path)
    : Visitor(src_manager, src_path) {
}

void FunctionASTConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
}

////////////////////////
// FunctionFrontendAction class
////////////////////////

FunctionFrontendAction::FunctionFrontendAction(const char *src_path)
    : src_path_(src_path) {
}

unique_ptr<clang::ASTConsumer> FunctionFrontendAction::CreateASTConsumer(
    clang::CompilerInstance &CI, llvm::StringRef InFile) {
  clang::SourceManager &source_manager = CI.getSourceManager();
  return make_unique<FunctionASTConsumer>(source_manager, src_path_);
}

void FunctionFrontendAction::ExecuteAction() {
  clang::ASTFrontendAction::ExecuteAction();
  return;
}

/////////////////////////
// main logic
/////////////////////////

// Execute clang and get the system include paths
// and add them to the compile args
static void add_system_include_paths(vector<string> &compile_args) {
  const char *cmd = "clang -E -x c++ - -v < /dev/null 2>&1";
  FILE       *fp = popen(cmd, "r");
  if (fp == NULL) {
    std::cerr << "Error: could not run command: " << cmd << "\n";
    return;
  }

  char buffer[256];
  bool found_include_start = false;
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    string line(buffer);
    if (found_include_start) {
      if (line.find("End of search list.") != string::npos) {
        found_include_start = false;
        continue;
      }

      if (line.find("starts here") != string::npos) { continue; }

      if (line.empty()) { continue; }

      if (line[0] == ' ') { line = line.substr(1); }

      const size_t len = line.length();

      if (line[len - 1] == '\n') { line = line.substr(0, len - 1); }

      compile_args.push_back("-isystem");
      compile_args.push_back(line);
      continue;
    }

    if (line.find("#include <...> search starts here") != string::npos) {
      found_include_start = true;
      continue;
    }

    if (line.find("#include \"...\" search starts here") != string::npos) {
      found_include_start = true;
      continue;
    }
  }
  pclose(fp);
  return;
}

int main(int argc, const char **argv) {
  if (argc < 1) {
    std::cerr << "Usage: " << argv[0]
              << " <source-file> -- [<compile args> ...]\n";
    return 1;
  }

  const string   src_path = argv[1];
  vector<string> compile_args;
  if (argc > 2) {
    if (strncmp(argv[2], "--", 2) != 0) {
      std::cerr << "Error: expected '--' after source file.\n";
      return 1;
    }

    for (int i = 3; i < argc; ++i) {
      compile_args.push_back(argv[i]);
    }
  }

  add_system_include_paths(compile_args);

#if PRINT_DEBUG == 1
  std::cerr << "Compile args:\n";
  for (const auto &arg : compile_args) {
    std::cerr << arg << " ";
  }
  std::cerr << "\n\n";
#endif

  ifstream src_file(src_path);

  if (!src_file.is_open()) {
    std::cerr << "Error: could not open source file " << src_path << "\n";
    return 1;
  }

  stringstream src_buffer;
  src_buffer << src_file.rdbuf();
  src_file.close();

  clang::tooling::runToolOnCodeWithArgs(
      make_unique<FunctionFrontendAction>(argv[1]), src_buffer.str(),
      compile_args, src_path);

  return 0;
}