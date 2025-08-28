#include "get_all_src.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "cpp_code_extractor_util.hpp"

static void add_element(Json::Value &dict, const string &file,
                        const string &type, const string &key,
                        const string &value) {
  if (!dict.isMember(file)) { dict[file] = Json::Value(Json::objectValue); }

  if (!dict[file].isMember(type)) {
    dict[file][type] = Json::Value(Json::objectValue);
  }

  dict[file][type][key] = value;
  return;
}

// /////////////////////////
// AllSrcVisitor class
// /////////////////////////

AllSrcVisitor::AllSrcVisitor(clang::SourceManager &src_manager,
                             clang::LangOptions   &lang_opts,
                             Json::Value          &output_json)
    : src_manager_(src_manager),
      lang_opts_(lang_opts),
      output_json_(output_json) {
}

bool AllSrcVisitor::VisitFunctionDecl(clang::FunctionDecl *FuncDecl) {
  if (!FuncDecl->isThisDeclarationADefinition()) { return true; }

  const string func_name = FuncDecl->getNameInfo().getName().getAsString();
  if (func_name == "") { return true; }

  clang::SourceLocation loc = FuncDecl->getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);
  const string          file_path = get_abs_path(file_name.str());

  if (file_path.empty()) {
    // Skip if the file path is empty
    return true;
  }

  if (is_system_file(file_path)) {
    // Skip system files
    return true;
  }

  // get function source code

  clang::SourceLocation start_loc = FuncDecl->getBeginLoc();
  clang::SourceLocation end_loc = FuncDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();

  add_element(output_json_, file_path, "functions", func_name, src_code);
  return true;
}

bool AllSrcVisitor::VisitVarDecl(clang::VarDecl *VarDecl) {
  const string var_name = VarDecl->getNameAsString();
  if (var_name == "") { return true; }

  clang::SourceLocation loc = VarDecl->getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);
  const string          file_path = get_abs_path(file_name.str());

  if (file_path.empty()) {
    // Skip if the file path is empty
    return true;
  }

  if (is_system_file(file_path)) {
    // Skip system files
    return true;
  }

  // get variable initialization source code
  clang::SourceLocation start_loc = VarDecl->getBeginLoc();
  clang::SourceLocation end_loc = VarDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();

  add_element(output_json_, file_path, "variables", var_name, src_code);

  return true;
}

bool AllSrcVisitor::VisitTypedefDecl(clang::TypedefDecl *TypedefDecl) {
  const string typedef_name = TypedefDecl->getNameAsString();
  if (typedef_name == "") { return true; }

  clang::SourceLocation loc = TypedefDecl->getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);
  const string          file_path = get_abs_path(file_name.str());

  if (file_path.empty()) {
    // Skip if the file path is empty
    return true;
  }

  if (is_system_file(file_path)) {
    // Skip system files
    return true;
  }

  // get typedef source code
  clang::SourceLocation start_loc = TypedefDecl->getBeginLoc();
  clang::SourceLocation end_loc = TypedefDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();

  add_element(output_json_, file_path, "typedefs", typedef_name, src_code);

  return true;
}

bool AllSrcVisitor::VisitRecordDecl(clang::RecordDecl *RecordDecl) {
  const string record_name = RecordDecl->getNameAsString();
  if (record_name == "") { return true; }

  clang::SourceLocation loc = RecordDecl->getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);
  const string          file_path = get_abs_path(file_name.str());

  if (file_path.empty()) {
    // Skip if the file path is empty
    return true;
  }

  if (is_system_file(file_path)) {
    // Skip system files
    return true;
  }

  // get record source code
  clang::SourceLocation start_loc = RecordDecl->getBeginLoc();
  clang::SourceLocation end_loc = RecordDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();

  add_element(output_json_, file_path, "records", record_name, src_code);

  return true;
}

bool AllSrcVisitor::VisitEnumDecl(clang::EnumDecl *EnumDecl) {
  const string enum_name = EnumDecl->getNameAsString();
  if (enum_name == "") { return true; }

  clang::SourceLocation loc = EnumDecl->getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);
  const string          file_path = get_abs_path(file_name.str());

  if (file_path.empty()) {
    // Skip if the file path is empty
    return true;
  }

  if (is_system_file(file_path)) {
    // Skip system files
    return true;
  }

  // get enum source code
  clang::SourceLocation start_loc = EnumDecl->getBeginLoc();
  clang::SourceLocation end_loc = EnumDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();

  add_element(output_json_, file_path, "enums", enum_name, src_code);

  return true;
}

/// ////////////////////////
// AllSrcASTConsumer class
// ////////////////////////

AllSrcASTConsumer::AllSrcASTConsumer(clang::SourceManager &src_manager,
                                     clang::LangOptions   &lang_opts,
                                     Json::Value          &output_json)
    : Visitor(src_manager, lang_opts, output_json) {
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
  clang::LangOptions   &lang_opts = CI.getLangOpts();

  return make_unique<AllSrcASTConsumer>(source_manager, lang_opts,
                                        output_json_);
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

  clang::SourceLocation loc = MacroNameTok.getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);

  if (file_name.empty()) { return; }
  const string file_path = get_abs_path(file_name.str());

  if (is_system_file(file_path)) {
    // Skip system files
    return;
  }

  const string macro_name = MacroNameTok.getIdentifierInfo()->getName().str();

  clang::SourceLocation DefBegin = MI->getDefinitionLoc();
  clang::SourceLocation DefEnd = MI->getDefinitionEndLoc();

  const string def =
      "#define " + clang::Lexer::getSourceText(
                       clang::CharSourceRange::getTokenRange(DefBegin, DefEnd),
                       src_manager_, lang_opts_)
                       .str();

  add_element(output_json_, file_path, "macros", macro_name, def);
  return;
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
