#include "get_src_range.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "cpp_code_extractor_util.hpp"

// /////////////////////////
// SrcRangeVisitor class
// /////////////////////////

SrcRangeVisitor::SrcRangeVisitor(clang::SourceManager &src_manager,
                                 clang::LangOptions   &lang_opts,
                                 llvm::StringRef       src_path,
                                 Json::Value          &output_json)
    : src_manager_(src_manager),
      lang_opts_(lang_opts),
      src_path_(src_path),
      output_json_(output_json) {
}

bool SrcRangeVisitor::VisitFunctionDecl(clang::FunctionDecl *FuncDecl) {
  if (!FuncDecl->isThisDeclarationADefinition()) { return true; }

  std::string func_name = FuncDecl->getNameInfo().getName().getAsString();

  clang::SourceLocation loc = FuncDecl->getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);
  const std::string     file_path = get_abs_path(file_name.str());

  if (file_path.empty()) {
    // Skip if the file path is empty
    std::cerr << "Warning: empty file path for function " << func_name << "\n";
    return true;
  }

  if (is_system_file(file_path)) {
    // Skip system files
    return true;
  }

  // get function source code

  clang::SourceLocation start_loc = FuncDecl->getBeginLoc();
  clang::PresumedLoc    presumed_loc = src_manager_.getPresumedLoc(start_loc);

  unsigned int start_line = presumed_loc.getLine();

  clang::SourceLocation end_loc = FuncDecl->getEndLoc();
  clang::PresumedLoc    end_presumed_loc = src_manager_.getPresumedLoc(end_loc);

  unsigned int end_line = end_presumed_loc.getLine();

  if (!output_json_.isMember(file_path)) {
    output_json_[file_path] = Json::Value(Json::objectValue);
  }

  output_json_[file_path][func_name] = Json::Value(Json::objectValue);
  output_json_[file_path][func_name]["begin"] = start_line;
  output_json_[file_path][func_name]["end"] = end_line;
  return true;
}

/// ////////////////////////
// SrcRangeASTConsumer class
// ////////////////////////

SrcRangeASTConsumer::SrcRangeASTConsumer(clang::SourceManager &src_manager,
                                         clang::LangOptions   &lang_opts,
                                         llvm::StringRef       src_path,
                                         Json::Value          &output_json)
    : Visitor(src_manager, lang_opts, src_path, output_json) {
}

void SrcRangeASTConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
}

// ////////////////////////
// SrcRangeFrontendAction class
// ////////////////////////

SrcRangeFrontendAction::SrcRangeFrontendAction(Json::Value &output_json)
    : output_json_(output_json) {
}

std::unique_ptr<clang::ASTConsumer> SrcRangeFrontendAction::CreateASTConsumer(
    clang::CompilerInstance &CI, llvm::StringRef InFile) {
  clang::SourceManager &source_manager = CI.getSourceManager();
  const clang::FileID   main_file_id = source_manager.getMainFileID();

  clang::OptionalFileEntryRef main_file_ref =
      source_manager.getFileEntryRefForID(main_file_id);

  if (!main_file_ref) {
    llvm::errs() << "Error: Main file entry not found for ID: "
                 << main_file_id.getHashValue() << "\n";
    return nullptr;
  }

  llvm::StringRef main_file_name = main_file_ref->getName();

  clang::LangOptions &lang_opts = CI.getLangOpts();

  return std::make_unique<SrcRangeASTConsumer>(source_manager, lang_opts,
                                               main_file_name, output_json_);
}

void SrcRangeFrontendAction::ExecuteAction() {
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

  const std::string src_path = argv[1];
  const char       *out_json = argv[2];

  const std::vector<std::string> compile_args = get_compile_args(argc, argv);

  std::ifstream src_file(src_path);

  if (!src_file.is_open()) {
    std::cerr << "Error: could not open source file " << src_path << "\n";
    return 1;
  }

  Json::Value output_json;

  std::stringstream src_buffer;
  src_buffer << src_file.rdbuf();
  src_file.close();

  clang::tooling::runToolOnCodeWithArgs(
      std::make_unique<SrcRangeFrontendAction>(output_json), src_buffer.str(),
      compile_args, src_path);

  std::ofstream output_file(out_json);
  if (!output_file.is_open()) {
    std::cerr << "Error: could not open output file " << out_json << "\n";
    return 1;
  }

  output_file << output_json.toStyledString();
  output_file.close();
  std::cout << "Result written to " << out_json << "\n";
  std::cout << "Total files found: " << output_json.size() << "\n";

  unsigned int num_functions = 0;
  for (const auto &file_entry : output_json.getMemberNames()) {
    const auto &file_functions = output_json[file_entry];
    num_functions += file_functions.size();
  }

  std::cout << "Total functions found: " << num_functions << "\n";

  return 0;
}
