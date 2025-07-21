#include "get_all_src.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "cpp_code_extractor_util.hpp"

// /////////////////////////
// AllSrcVisitor class
// /////////////////////////

AllSrcVisitor::AllSrcVisitor(clang::SourceManager &src_manager,
                             clang::LangOptions   &lang_opts,
                             llvm::StringRef src_path, Json::Value &output_json)
    : src_manager_(src_manager),
      lang_opts_(lang_opts),
      src_path_(src_path),
      output_json_(output_json) {
}

bool AllSrcVisitor::VisitFunctionDecl(clang::FunctionDecl *FuncDecl) {
  if (!FuncDecl->isThisDeclarationADefinition()) { return true; }

  string func_name = FuncDecl->getNameInfo().getName().getAsString();

  clang::SourceLocation loc = FuncDecl->getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);

  if (file_name != src_path_) { return true; }

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

bool AllSrcVisitor::VisitVarDecl(clang::VarDecl *VarDecl) {
  string var_name = VarDecl->getNameAsString();

  // get variable initialization source code
  clang::SourceLocation start_loc = VarDecl->getBeginLoc();
  clang::SourceLocation end_loc = VarDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();

  output_json_[var_name] = src_code;

  return true;
}

bool AllSrcVisitor::VisitTypedefDecl(clang::TypedefDecl *TypedefDecl) {
  string typedef_name = TypedefDecl->getNameAsString();

  // get typedef source code
  clang::SourceLocation start_loc = TypedefDecl->getBeginLoc();
  clang::SourceLocation end_loc = TypedefDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();

  output_json_[typedef_name] = src_code;

  return true;
}

bool AllSrcVisitor::VisitRecordDecl(clang::RecordDecl *RecordDecl) {
  string record_name = RecordDecl->getNameAsString();

  // get record source code
  clang::SourceLocation start_loc = RecordDecl->getBeginLoc();
  clang::SourceLocation end_loc = RecordDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();

  output_json_[record_name] = src_code;

  return true;
}

bool AllSrcVisitor::VisitEnumDecl(clang::EnumDecl *EnumDecl) {
  string enum_name = EnumDecl->getNameAsString();

  // get enum source code
  clang::SourceLocation start_loc = EnumDecl->getBeginLoc();
  clang::SourceLocation end_loc = EnumDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();

  output_json_[enum_name] = src_code;

  return true;
}

/// ////////////////////////
// AllSrcASTConsumer class
// ////////////////////////

AllSrcASTConsumer::AllSrcASTConsumer(clang::SourceManager &src_manager,
                                     clang::LangOptions   &lang_opts,
                                     llvm::StringRef       src_path,
                                     Json::Value          &output_json)
    : Visitor(src_manager, lang_opts, src_path, output_json) {
}

void AllSrcASTConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
}

// ////////////////////////
// AllSrcFrontendAction class
// ////////////////////////

AllSrcFrontendAction::AllSrcFrontendAction(Json::Value &output_json)
    : output_json_(output_json) {
}
unique_ptr<clang::ASTConsumer> AllSrcFrontendAction::CreateASTConsumer(
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

  return make_unique<AllSrcASTConsumer>(source_manager, lang_opts,
                                        main_file_name, output_json_);
}

void AllSrcFrontendAction::ExecuteAction() {
  clang::ASTFrontendAction::ExecuteAction();
  return;
}

// ////////////////////////
// MacroPrinter class
// ////////////////////////

MacroPrinter::MacroPrinter(clang::SourceManager &src_manager,
                           clang::LangOptions   &lang_opts,
                           Json::Value          &output_json)
    : src_manager_(src_manager),
      lang_opts_(lang_opts),
      output_json_(output_json) {
}

void MacroPrinter::MacroDefined(const clang::Token          &MacroNameTok,
                                const clang::MacroDirective *MD) {
  const clang::MacroInfo *MI = MD->getMacroInfo();

  const string macro_name = MacroNameTok.getIdentifierInfo()->getName().str();
  string       def = "#define " + macro_name;

  // get macro arguments
  if (!MI->param_empty()) {
    def += "(";
    auto iter = MI->param_begin();
    while (iter != MI->param_end()) {
      if (iter != MI->param_begin()) { def += ", "; }
      def += (*iter)->getName().str();
      ++iter;
    }
    def += ")";
  }

  def += " ";

  for (const clang::Token &Tok : MI->tokens()) {
    clang::SourceLocation TokLoc = Tok.getLocation();
    const char           *data = src_manager_.getCharacterData(TokLoc);
    const unsigned int    length = Tok.getLength();
    def += string(data, length);
  }
  output_json_[macro_name] = def;
}

// ////////////////////////
// MacroAction class
// ////////////////////////

MacroAction::MacroAction(Json::Value &output_json) : output_json_(output_json) {
}

void MacroAction::ExecuteAction() {
  clang::CompilerInstance &CI = getCompilerInstance();
  clang::Preprocessor     &PP = CI.getPreprocessor();
  clang::SourceManager    &SM = PP.getSourceManager();
  clang::LangOptions      &lang_opts = CI.getLangOpts();

  PP.addPPCallbacks(make_unique<MacroPrinter>(SM, lang_opts, output_json_));

  PP.EnterMainSourceFile();
  clang::Token Tok;

  do {
    PP.Lex(Tok);
  } while (Tok.isNot(clang::tok::eof));

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

  const vector<string> compile_args = get_compile_args(argc, argv);

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
      make_unique<AllSrcFrontendAction>(output_json), src_buffer.str(),
      compile_args, src_path);

  clang::tooling::runToolOnCodeWithArgs(make_unique<MacroAction>(output_json),
                                        src_buffer.str(), compile_args,
                                        src_path);

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
