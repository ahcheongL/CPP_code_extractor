#include "get_func_list.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include "util.hpp"

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

  std::cout << func_name << "\n";

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

FunctionFrontendAction::FunctionFrontendAction() {
}

unique_ptr<clang::ASTConsumer> FunctionFrontendAction::CreateASTConsumer(
    clang::CompilerInstance &CI, llvm::StringRef InFile) {
  clang::SourceManager   &source_manager = CI.getSourceManager();
  const clang::FileID     main_file_id = source_manager.getMainFileID();
  const clang::FileEntry *main_file_entry =
      source_manager.getFileEntryForID(main_file_id);
  const char *main_file_name = main_file_entry->getName().data();
  return make_unique<FunctionASTConsumer>(source_manager, main_file_name);
}

void FunctionFrontendAction::ExecuteAction() {
  clang::ASTFrontendAction::ExecuteAction();
  return;
}

/////////////////////////
// main logic
/////////////////////////

int main(int argc, const char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0]
              << " <source-file> -- [<compile args> ...]\n";
    return 1;
  }

  const char    *src_path = argv[1];
  vector<string> compile_args;
  if (argc > 3) {
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

  clang::tooling::runToolOnCodeWithArgs(make_unique<FunctionFrontendAction>(),
                                        src_buffer.str(), compile_args,
                                        src_path);

  return 0;
}