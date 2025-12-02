#include "get_all_src.hpp"

#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "cpp_code_extractor_util.hpp"


static void add_object(Json::Value *root,
                     const std::vector<std::string> &path,
                     const Json::Value &value) {
    Json::Value* current = root;

    for (size_t i = 0; i < path.size()-1; i++) {
        const std::string &key = path[i];
        if (!current->isMember(key)) {
            (*current)[key] = Json::Value(Json::objectValue);
        }
        current = &((*current)[key]);
    }

    const std::string &last_key = path.back();
    (*current)[last_key] = Json::Value(value);
}

static void add_list(Json::Value *root,
                     const std::vector<std::string> &path,
                     const Json::Value &value) {
    Json::Value* current = root;

    for (size_t i = 0; i < path.size()-1; i++) {
        const std::string &key = path[i];
        if (!current->isMember(key)) {
            (*current)[key] = Json::Value(Json::objectValue);
        }
        current = &((*current)[key]);
    }

    const std::string &last_key = path.back();
    if (!current->isMember(last_key)) {
        (*current)[last_key] = Json::Value(Json::arrayValue);
    }
    (*current)[last_key].append(Json::Value(value));
}

static string get_macro_name(const string &line) {
  size_t pos = line.find("define");
  if (pos == string::npos) { return ""; }
  string name = line.substr(pos + 7);
  pos = name.find(' ');
  if (pos != string::npos) { name = name.substr(0, pos); }
  pos = name.find('(');
  if (pos != string::npos) { name = name.substr(0, pos); }
  name = strip(name);
  return name;
}

static void collect_disabled_macros(Json::Value  &output_json,
                                    const string &file_path) {
  static set<string> visited_files;
  if (visited_files.find(file_path) != visited_files.end()) { return; }
  visited_files.insert(file_path);

  ifstream file(file_path);
  if (!file.is_open()) {
    llvm::outs() << "Failed to open file: " << file_path << "\n";
    return;
  }

  regex pattern(R"(^\s*#\s*define\b)");

  string line;
  string macro_line = "";
  int line_no = 0;
  while (getline(file, line)) {
    line_no++;
    if (!regex_search(line, pattern)) { continue; }

    macro_line = strip(line);
    int line_start_no = line_no - 1;
    while (ends_with(line, "\\")) {
      getline(file, line);
      line_no++;
      macro_line += "\n" + strip(line);
    }
    int line_end_no = line_no;
    const string macro_name = get_macro_name(macro_line);

    if (!macro_name.empty()) {
      Json::Value macro_info;
      macro_info["code"] = macro_line;
      macro_info["line_start"] = line_start_no;
      macro_info["line_end"] = line_end_no;
      add_list(&output_json, {file_path, "disabled_macros", macro_name}, macro_info);
    }
  }

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

  collect_disabled_macros(output_json_, file_path);

  // get function source code

  clang::SourceLocation start_loc = FuncDecl->getBeginLoc();
  clang::SourceLocation end_loc = FuncDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();
  const int line_start_no =  src_manager_.getSpellingLineNumber(start_loc);
  const int line_end_no = src_manager_.getSpellingLineNumber(end_loc);
  Json::Value func_info;
  func_info["code"] = src_code;
  func_info["line_start"] = line_start_no;
  func_info["line_end"] = line_end_no;
  add_object(&output_json_, {file_path, "functions", func_name}, func_info);

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

  collect_disabled_macros(output_json_, file_path);

  // get variable initialization source code
  clang::SourceLocation start_loc = VarDecl->getBeginLoc();
  clang::SourceLocation end_loc = VarDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();
  const int line_start_no =  src_manager_.getSpellingLineNumber(start_loc);
  const int line_end_no = src_manager_.getSpellingLineNumber(end_loc);

  Json::Value var_info;
  var_info["code"] = src_code;
  var_info["line_start"] = line_start_no;
  var_info["line_end"] = line_end_no;
  
  if (VarDecl->hasGlobalStorage()) {
    add_object(&output_json_, {file_path, "global_variables", var_name}, var_info);
  }
  else if (VarDecl->isLocalVarDeclOrParm() ) {
    if ( VarDecl->getLexicalDeclContext()->isFunctionOrMethod() ) {
          clang::FunctionDecl* func_decl = static_cast<clang::FunctionDecl*>(VarDecl->getLexicalDeclContext());
          const string func_name = func_decl->getNameInfo().getName().getAsString();
          add_object(&output_json_, {file_path, "functions", func_name, "local_variables", var_name }, var_info);
    }
  }
  else {
    add_object(&output_json_, {file_path, "variables", var_name}, var_info);
  }

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

  collect_disabled_macros(output_json_, file_path);

  // get typedef source code
  clang::SourceLocation start_loc = TypedefDecl->getBeginLoc();
  clang::SourceLocation end_loc = TypedefDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();
  const int line_start_no =  src_manager_.getSpellingLineNumber(start_loc);
  const int line_end_no = src_manager_.getSpellingLineNumber(end_loc);

  Json::Value typedef_info;
  typedef_info["code"] = src_code;
  typedef_info["line_start"] = line_start_no;
  typedef_info["line_end"] = line_end_no;

  add_object(&output_json_, {file_path, "typedefs", typedef_name}, typedef_info);

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

  collect_disabled_macros(output_json_, file_path);

  // get record source code
  clang::SourceLocation start_loc = RecordDecl->getBeginLoc();
  clang::SourceLocation end_loc = RecordDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();
  const int line_start_no =  src_manager_.getSpellingLineNumber(start_loc);
  const int line_end_no = src_manager_.getSpellingLineNumber(end_loc);

  Json::Value record_info;
  record_info["code"] = src_code;
  record_info["line_start"] = line_start_no;
  record_info["line_end"] = line_end_no;

  add_object(&output_json_, {file_path, "records", record_name}, record_info);
  
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

  collect_disabled_macros(output_json_, file_path);

  // get enum source code
  clang::SourceLocation start_loc = EnumDecl->getBeginLoc();
  clang::SourceLocation end_loc = EnumDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();
  const int line_start_no =  src_manager_.getSpellingLineNumber(start_loc);
  const int line_end_no = src_manager_.getSpellingLineNumber(end_loc);

  Json::Value enum_info;
  enum_info["code"] = src_code;
  enum_info["line_start"] = line_start_no;
  enum_info["line_end"] = line_end_no;

  add_object(&output_json_, {file_path, "enums", enum_name}, enum_info);

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
  collect_disabled_macros(output_json_, file_path);

  const string macro_name = MacroNameTok.getIdentifierInfo()->getName().str();

  clang::SourceLocation DefBegin = MI->getDefinitionLoc();
  clang::SourceLocation DefEnd = MI->getDefinitionEndLoc();

  const string def =
      "#define " + clang::Lexer::getSourceText(
                       clang::CharSourceRange::getTokenRange(DefBegin, DefEnd),
                       src_manager_, lang_opts_)
                       .str();
  const int line_start_no =  src_manager_.getSpellingLineNumber(DefBegin);
  const int line_end_no = src_manager_.getSpellingLineNumber(DefEnd);

  Json::Value macro_info;
  macro_info["code"] = def;
  macro_info["line_start"] = line_start_no;
  macro_info["line_end"] = line_end_no;

  add_object(&output_json_, {file_path, "macros", macro_name}, macro_info);
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

void remove_enabled_macros(Json::Value &output_json) {
  for (const string &file_name : output_json.getMemberNames()) {
    if (!output_json[file_name].isMember("macros")) { continue; }

    Json::Value &enabled_macros = output_json[file_name]["macros"];
    Json::Value &disabled_macros = output_json[file_name]["disabled_macros"];

    for (const string &macro_name : disabled_macros.getMemberNames()) {
      if (!enabled_macros.isMember(macro_name)) { continue; }
      const string &enabled_def = enabled_macros[macro_name]["code"].asString();

      Json::Value &disabled_defs = disabled_macros[macro_name];

      Json::Value remained = Json::Value(Json::arrayValue);

      for (const Json::Value &disabled_def : disabled_defs) {
        const string disabled_str = disabled_def["code"].asString();

        if (disabled_str == enabled_def) { continue; }
        remained.append(disabled_def);
      }

      disabled_macros[macro_name] = remained;
    }

    Json::Value remained = Json::Value(Json::objectValue);

    for (const string &macro_name : disabled_macros.getMemberNames()) {
      Json::Value &disabled_defs = disabled_macros[macro_name];
      if (disabled_defs.size() == 0) { continue; }
      remained[macro_name] = disabled_defs;
    }

    output_json[file_name]["disabled_macros"] = remained;
  }
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

  remove_enabled_macros(output_json);

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
