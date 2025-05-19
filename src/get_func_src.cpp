#include "get_func_src.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "util.hpp"

// /////////////////////////
// FuncSrcVisitor class
// /////////////////////////

FuncSrcVisitor::FuncSrcVisitor(clang::SourceManager &src_manager,
                               clang::LangOptions   &lang_opts,
                               const char *src_path, const char *target_func)
    : src_manager_(src_manager),
      lang_opts_(lang_opts),
      src_path_(src_path),
      target_func_(target_func) {
}

bool FuncSrcVisitor::VisitFunctionDecl(clang::FunctionDecl *FuncDecl) {
  if (!FuncDecl->isThisDeclarationADefinition()) { return true; }

  string func_name = FuncDecl->getNameInfo().getName().getAsString();

  if (func_name != target_func_) { return true; }

  clang::SourceLocation loc = FuncDecl->getLocation();

  // get filename
  const clang::FileEntry *file_entry =
      src_manager_.getFileEntryForID(src_manager_.getFileID(loc));
  if (file_entry == nullptr) { return true; }

  const char *file_name = file_entry->getName().data();
  if (strcmp(file_name, src_path_) != 0) { return true; }

  // get function source code

  clang::SourceLocation start_loc = FuncDecl->getBeginLoc();
  clang::SourceLocation end_loc = FuncDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();

  std::cout << src_code << "\n";

  return true;
}

/// ////////////////////////
// FuncSrcASTConsumer class
// ////////////////////////

FuncSrcASTConsumer::FuncSrcASTConsumer(clang::SourceManager &src_manager,
                                       clang::LangOptions   &lang_opts,
                                       const char           *src_path,
                                       const char           *target_func)
    : Visitor(src_manager, lang_opts, src_path, target_func) {
}

void FuncSrcASTConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
}

// ////////////////////////
// FuncSrcFrontendAction class
// ////////////////////////

FuncSrcFrontendAction::FuncSrcFrontendAction(const char *target_func)
    : target_func_(target_func) {
}
unique_ptr<clang::ASTConsumer> FuncSrcFrontendAction::CreateASTConsumer(
    clang::CompilerInstance &CI, llvm::StringRef InFile) {
  clang::SourceManager   &source_manager = CI.getSourceManager();
  const clang::FileID     main_file_id = source_manager.getMainFileID();
  const clang::FileEntry *main_file_entry =
      source_manager.getFileEntryForID(main_file_id);
  const char *main_file_name = main_file_entry->getName().data();

  clang::LangOptions &lang_opts = CI.getLangOpts();

  return make_unique<FuncSrcASTConsumer>(source_manager, lang_opts,
                                         main_file_name, target_func_);
}

void FuncSrcFrontendAction::ExecuteAction() {
  clang::ASTFrontendAction::ExecuteAction();
  return;
}

// ////////////////////////
// // main function
// ////////////////////////

int main(int argc, const char **argv) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <source-file> <func_name> -- [<compile args> ...]\n";
    return 1;
  }

  const string src_path = argv[1];
  const char  *func_name = argv[2];

  vector<string> compile_args;

  if (argc > 4) {
    if (strncmp(argv[3], "--", 2) != 0) {
      std::cerr << "Error: expected '--' after func name.\n";
      return 1;
    }

    for (int i = 4; i < argc; ++i) {
      compile_args.push_back(argv[i]);
    }
  }

  add_system_include_paths(compile_args);

  ifstream src_file(src_path);

  if (!src_file.is_open()) {
    std::cerr << "Error: could not open source file " << src_path << "\n";
    return 1;
  }

  stringstream src_buffer;
  src_buffer << src_file.rdbuf();
  src_file.close();

  clang::tooling::runToolOnCodeWithArgs(
      make_unique<FuncSrcFrontendAction>(func_name), src_buffer.str(),
      compile_args, src_path);

  return 0;
}
