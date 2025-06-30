#include "parse_cpp.hpp"

#include <fstream>
#include <iostream>

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include "cpp_code_extractor_util.hpp"

// /////////////////////////
// ParseASTConsumer class
// /////////////////////////

ParseASTConsumer::ParseASTConsumer(clang::SourceManager &src_manager,
                                   clang::LangOptions   &lang_opts,
                                   const char           *src_path,
                                   Json::Value          &output_json)
    : src_manager_(src_manager),
      lang_opts_(lang_opts),
      src_path_(src_path),
      output_json_(output_json) {
}

void ParseASTConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
  clang::TranslationUnitDecl *tu_decl = Context.getTranslationUnitDecl();

  for (auto *decl : tu_decl->decls()) {
    if (decl->isImplicit()) { continue; }
    if (!llvm::isa<clang::NamedDecl>(decl)) { continue; }

    clang::NamedDecl *named_decl = llvm::dyn_cast<clang::NamedDecl>(decl);

    clang::SourceLocation   loc = named_decl->getLocation();
    const clang::FileEntry *file_entry =
        src_manager_.getFileEntryForID(src_manager_.getFileID(loc));
    if (file_entry == nullptr) { continue; }

    const char *file_name = file_entry->getName().data();

    if (strcmp(file_name, src_path_) != 0) { continue; }

    // named_decl->print(llvm::errs());
    // llvm::errs() << "\n";

    const string decl_name = named_decl->getNameAsString();

    clang::SourceLocation start_loc = named_decl->getBeginLoc();
    clang::SourceLocation end_loc = named_decl->getEndLoc();
    clang::SourceRange    range(start_loc, end_loc);

    const string src_code = clang::Lexer::getSourceText(
                                clang::CharSourceRange::getTokenRange(range),
                                src_manager_, lang_opts_)
                                .str();

    const bool is_func_def = llvm::isa<clang::FunctionDecl>(named_decl) &&
                             llvm::cast<clang::FunctionDecl>(named_decl)
                                 ->isThisDeclarationADefinition();

    Json::Value decl_elem = Json::Value(Json::objectValue);
    decl_elem["name"] = decl_name;
    decl_elem["is_func_def"] = is_func_def;

    if (!is_func_def) {
      decl_elem["source"] = src_code + ";";
    } else {
      decl_elem["source"] = src_code;
    }

    output_json_.append(decl_elem);
  }
  return;
}

// /////////////////////////
// ParseFrontendAction class
// /////////////////////////

ParseFrontendAction::ParseFrontendAction(Json::Value &output_json)
    : output_json_(output_json) {
}

std::unique_ptr<clang::ASTConsumer> ParseFrontendAction::CreateASTConsumer(
    clang::CompilerInstance &CI, llvm::StringRef InFile) {
  clang::SourceManager &src_manager = CI.getSourceManager();
  clang::LangOptions   &lang_opts = CI.getLangOpts();
  const char           *main_file_name =
      src_manager.getFileEntryForID(src_manager.getMainFileID())
          ->getName()
          .data();

  return std::make_unique<ParseASTConsumer>(src_manager, lang_opts,
                                            main_file_name, output_json_);
}

void ParseFrontendAction::ExecuteAction() {
  clang::ASTFrontendAction::ExecuteAction();
  return;
}

// /////////////////////////
// main function
// /////////////////////////

int main(int argc, const char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <source-file> <out.json>\n";
    return 1;
  }

  const string src_path = argv[1];
  const char  *output_filename = argv[2];

  const vector<string> compile_args = get_compile_args(argc, argv);

  ifstream src_file(src_path);

  if (!src_file.is_open()) {
    std::cerr << "Error: could not open source file " << src_path << "\n";
    return 1;
  }

  Json::Value output_json = Json::Value(Json::arrayValue);

  stringstream src_buffer;
  src_buffer << src_file.rdbuf();
  src_file.close();

  clang::tooling::runToolOnCodeWithArgs(
      make_unique<ParseFrontendAction>(output_json), src_buffer.str(),
      compile_args, src_path);

  ofstream output_file(output_filename);
  if (!output_file.is_open()) {
    std::cerr << "Error: could not open output file " << output_filename
              << "\n";
    return 1;
  }

  output_file << output_json.toStyledString();
  output_file.close();
  std::cout << "Function call graph extracted to " << output_filename << "\n";
  std::cout << "Total functions found: " << output_json.size() << "\n";

  return 0;
}