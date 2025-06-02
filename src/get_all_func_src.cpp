#include "get_all_func_src.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "util.hpp"

// /////////////////////////
// AllFuncSrcVisitor class
// /////////////////////////

AllFuncSrcVisitor::AllFuncSrcVisitor(clang::SourceManager &src_manager,
                                     clang::LangOptions   &lang_opts,
                                     const char           *src_path,
                                     Json::Value          &output_json)
    : src_manager_(src_manager),
      lang_opts_(lang_opts),
      src_path_(src_path),
      output_json_(output_json) {
}

bool AllFuncSrcVisitor::VisitFunctionDecl(clang::FunctionDecl *FuncDecl) {
  if (!FuncDecl->isThisDeclarationADefinition()) { return true; }

  string func_name = FuncDecl->getNameInfo().getName().getAsString();

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

  output_json_[func_name] = src_code;

  return true;
}

/// ////////////////////////
// AllFuncSrcASTConsumer class
// ////////////////////////

AllFuncSrcASTConsumer::AllFuncSrcASTConsumer(clang::SourceManager &src_manager,
                                             clang::LangOptions   &lang_opts,
                                             const char           *src_path,
                                             Json::Value          &output_json)
    : Visitor(src_manager, lang_opts, src_path, output_json) {
}

void AllFuncSrcASTConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
}

// ////////////////////////
// AllFuncSrcFrontendAction class
// ////////////////////////

AllFuncSrcFrontendAction::AllFuncSrcFrontendAction(Json::Value &output_json)
    : output_json_(output_json) {
}
unique_ptr<clang::ASTConsumer> AllFuncSrcFrontendAction::CreateASTConsumer(
    clang::CompilerInstance &CI, llvm::StringRef InFile) {
  clang::SourceManager   &source_manager = CI.getSourceManager();
  const clang::FileID     main_file_id = source_manager.getMainFileID();
  const clang::FileEntry *main_file_entry =
      source_manager.getFileEntryForID(main_file_id);
  const char *main_file_name = main_file_entry->getName().data();

  clang::LangOptions &lang_opts = CI.getLangOpts();

  return make_unique<AllFuncSrcASTConsumer>(source_manager, lang_opts,
                                            main_file_name, output_json_);
}

void AllFuncSrcFrontendAction::ExecuteAction() {
  clang::ASTFrontendAction::ExecuteAction();
  return;
}

// ////////////////////////
// // main function
// ////////////////////////

int main(int argc, const char **argv) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <source-file> <out.json> -- [<compile args> ...]\n";
    return 1;
  }

  const string src_path = argv[1];
  const char  *output_filename = argv[2];

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

  Json::Value output_json;

  stringstream src_buffer;
  src_buffer << src_file.rdbuf();
  src_file.close();

  clang::tooling::runToolOnCodeWithArgs(
      make_unique<AllFuncSrcFrontendAction>(output_json), src_buffer.str(),
      compile_args, src_path);

  ofstream output_file(output_filename);
  if (!output_file.is_open()) {
    std::cerr << "Error: could not open output file " << output_filename
              << "\n";
    return 1;
  }

  output_file << output_json.toStyledString();
  output_file.close();
  std::cout << "Function source code extracted to " << output_filename << "\n";
  std::cout << "Total functions found: " << output_json.size() << "\n";

  return 0;
}
