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

static string get_macro_name(const string &line) {
  istringstream iss(line);
  string        define, name;
  iss >> define >> name;

  if (define != "#define") { return ""; }

  // remove parameters if any
  size_t paren = name.find('(');
  if (paren != std::string::npos) name = name.substr(0, paren);
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

  string line;
  string macro_line = "";
  while (getline(file, line)) {
    if (line.find("#define") == string::npos) { continue; }

    macro_line = strip(line);
    while (ends_with(line, "\\")) {
      getline(file, line);
      macro_line += "\n" + strip(line);
    }

    const string macro_name = get_macro_name(macro_line);

    if (!macro_name.empty()) {
      if (!output_json.isMember(file_path)) {
        output_json[file_path] = Json::Value(Json::objectValue);
      }

      if (!output_json[file_path].isMember("disabled_macros")) {
        output_json[file_path]["disabled_macros"] =
            Json::Value(Json::objectValue);
      }

      if (!output_json[file_path]["disabled_macros"].isMember(macro_name)) {
        output_json[file_path]["disabled_macros"][macro_name] =
            Json::Value(Json::arrayValue);
      }

      output_json[file_path]["disabled_macros"][macro_name].append(macro_line);
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

  collect_disabled_macros(output_json_, file_path);

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

  collect_disabled_macros(output_json_, file_path);

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

  collect_disabled_macros(output_json_, file_path);

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

  collect_disabled_macros(output_json_, file_path);

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

void remove_enabled_macros(Json::Value &output_json) {
  for (const string &file_name : output_json.getMemberNames()) {
    if (!output_json[file_name].isMember("macros")) { continue; }

    Json::Value &enabled_macros = output_json[file_name]["macros"];
    Json::Value &disabled_macros = output_json[file_name]["disabled_macros"];

    for (const string &macro_name : disabled_macros.getMemberNames()) {
      if (!enabled_macros.isMember(macro_name)) { continue; }
      const string &enabled_def = enabled_macros[macro_name].asString();

      Json::Value &disabled_defs = disabled_macros[macro_name];

      Json::Value remained = Json::Value(Json::arrayValue);

      for (const Json::Value &disabled_def : disabled_defs) {
        const string disabled_str = disabled_def.asString();

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
