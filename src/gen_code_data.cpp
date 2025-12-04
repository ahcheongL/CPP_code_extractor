#include "gen_code_data.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>

#include "CompileCommand.hpp"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "cpp_code_extractor_util.hpp"
#include "json_utils.hpp"

static std::string get_macro_name(const std::string &line) {
  size_t pos = line.find("define");
  if (pos == std::string::npos) { return ""; }
  std::string name = line.substr(pos + 7);
  pos = name.find(' ');
  if (pos != std::string::npos) { name = name.substr(0, pos); }
  pos = name.find('\t');
  if (pos != std::string::npos) { name = name.substr(0, pos); }
  pos = name.find('(');
  if (pos != std::string::npos) { name = name.substr(0, pos); }
  name = strip(name);
  return name;
}

static void collect_disabled_macros(Json::Value       &output_json,
                                    const std::string &file_path) {
  static std::set<std::string> visited_files;
  if (visited_files.find(file_path) != visited_files.end()) { return; }
  visited_files.insert(file_path);

  std::ifstream file(file_path);
  if (!file.is_open()) {
    llvm::outs() << "Failed to open file: " << file_path << "\n";
    return;
  }

  ensure_key(output_json, file_path);
  Json::Value &file_entry = output_json[file_path];

  file_entry["disabled_macros"] = Json::Value(Json::objectValue);
  Json::Value &disabled_macros = file_entry["disabled_macros"];

  std::regex pattern(R"(^\s*#\s*define\b)");

  std::string line;
  std::string macro_line = "";
  int32_t     line_no = 0;
  while (getline(file, line)) {
    line_no++;
    if (!std::regex_search(line, pattern)) { continue; }

    macro_line = strip(line);
    int32_t start_line_no = line_no - 1;
    while (ends_with(line, "\\")) {
      getline(file, line);
      line_no++;
      macro_line += "\n" + strip(line);
    }
    int32_t           end_line_no = line_no;
    const std::string macro_name = get_macro_name(macro_line);

    if (macro_name.empty()) { continue; }

    if (!disabled_macros.isMember(macro_name)) {
      disabled_macros[macro_name] = Json::Value(Json::arrayValue);
    }

    Json::Value &macro_defs = disabled_macros[macro_name];

    Json::Value macro_info;
    macro_info["definition"] = macro_line;
    macro_info["start_line"] = start_line_no;
    macro_info["end_line"] = end_line_no;

    macro_defs.append(macro_info);
  }

  return;
}

static std::set<std::string> excludes;
static void                  get_excludes() {
  const char *env_val = std::getenv("EXCLUDES");
  if (env_val == nullptr) { return; }

  std::string              excludes_str(env_val);
  std::vector<std::string> tokens = tokenize_command(excludes_str);
  for (const std::string &excl : tokens) {
    excludes.insert(excl);
  }

  std::cout << "Found " << excludes.size() << " exclude fragments.\n";

  return;
}

// /////////////////////////
// CodeDataVisitor class
// /////////////////////////
bool CodeDataVisitor::VisitFunctionDecl(clang::FunctionDecl *FuncDecl) {
  if (!FuncDecl->isThisDeclarationADefinition()) { return true; }

  const std::string func_name = FuncDecl->getNameInfo().getName().getAsString();
  if (func_name == "") { return true; }

  clang::SourceLocation loc = FuncDecl->getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);
  if (file_name == "") { return true; }

  const std::string file_path = get_canonical_abs_path(file_name.str());

  if (is_system_file(file_path)) { return true; }

  ensure_file_key(output_json_, file_path);

  collect_disabled_macros(output_json_, file_path);

  Json::Value &file_entry = output_json_[file_path];
  Json::Value &functions_entry = file_entry["functions"];

  // get function source code

  clang::SourceLocation start_loc = FuncDecl->getBeginLoc();
  clang::SourceLocation end_loc = FuncDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const std::string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();

  const int32_t start_line_no = src_manager_.getSpellingLineNumber(start_loc);
  const int32_t end_line_no = src_manager_.getSpellingLineNumber(end_loc);

  ensure_key(functions_entry, func_name);
  Json::Value &func_entry = functions_entry[func_name];
  func_entry["definition"] = src_code;
  func_entry["start_line"] = start_line_no;
  func_entry["end_line"] = end_line_no;
  return true;
}

bool CodeDataVisitor::VisitVarDecl(clang::VarDecl *VarDecl) {
  const std::string var_name = VarDecl->getNameAsString();
  if (var_name == "") { return true; }

  clang::SourceLocation loc = VarDecl->getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);
  const std::string     file_path = get_canonical_abs_path(file_name.str());

  if (file_path.empty()) {
    // Skip if the file path is empty
    return true;
  }

  if (is_system_file(file_path)) {
    // Skip system files
    return true;
  }

  ensure_file_key(output_json_, file_path);
  collect_disabled_macros(output_json_, file_path);

  Json::Value &file_entry = output_json_[file_path];

  // get variable initialization source code
  clang::SourceLocation start_loc = VarDecl->getBeginLoc();
  clang::SourceLocation end_loc = VarDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const std::string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();
  const int32_t start_line_no = src_manager_.getSpellingLineNumber(start_loc);
  const int32_t end_line_no = src_manager_.getSpellingLineNumber(end_loc);

  if (VarDecl->isLocalVarDeclOrParm()) {
    clang::DeclContext *decl_ctxt = VarDecl->getLexicalDeclContext();

    clang::FunctionDecl *func_decl =
        clang::dyn_cast<clang::FunctionDecl>(decl_ctxt);

    if (func_decl != nullptr) {
      Json::Value      &functions_entry = output_json_[file_path]["functions"];
      const std::string func_name =
          func_decl->getNameInfo().getName().getAsString();

      ensure_key(functions_entry, func_name);
      Json::Value &func_entry = functions_entry[func_name];
      ensure_key(func_entry, "variables");
      Json::Value &variables_entry = func_entry["variables"];
      ensure_key(variables_entry, var_name);
      Json::Value &var_entry = variables_entry[var_name];
      var_entry["definition"] = src_code;
      var_entry["start_line"] = start_line_no;
      var_entry["end_line"] = end_line_no;
    }
    return true;
  }

  ensure_key(file_entry, "global_variables");
  Json::Value &global_vars_entry = file_entry["global_variables"];
  ensure_key(global_vars_entry, var_name);
  Json::Value &var_info = global_vars_entry[var_name];
  var_info["definition"] = src_code;
  var_info["start_line"] = start_line_no;
  var_info["end_line"] = end_line_no;
  return true;
}

bool CodeDataVisitor::VisitTypedefDecl(clang::TypedefDecl *TypedefDecl) {
  const std::string typedef_name = TypedefDecl->getNameAsString();
  if (typedef_name == "") { return true; }

  clang::SourceLocation loc = TypedefDecl->getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);
  const std::string     file_path = get_canonical_abs_path(file_name.str());

  if (file_path.empty()) {
    // Skip if the file path is empty
    return true;
  }

  if (is_system_file(file_path)) {
    // Skip system files
    return true;
  }

  ensure_file_key(output_json_, file_path);

  collect_disabled_macros(output_json_, file_path);

  Json::Value &file_entry = output_json_[file_path];
  Json::Value &types_entry = file_entry["types"];

  // get typedef source code
  clang::SourceLocation start_loc = TypedefDecl->getBeginLoc();
  clang::SourceLocation end_loc = TypedefDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const std::string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();
  const int32_t start_line_no = src_manager_.getSpellingLineNumber(start_loc);
  const int32_t end_line_no = src_manager_.getSpellingLineNumber(end_loc);

  ensure_key(types_entry, typedef_name);

  Json::Value &typedef_info = types_entry[typedef_name];
  typedef_info["definition"] = src_code;
  typedef_info["start_line"] = start_line_no;
  typedef_info["end_line"] = end_line_no;

  return true;
}

bool CodeDataVisitor::VisitRecordDecl(clang::RecordDecl *RecordDecl) {
  const std::string record_name = RecordDecl->getNameAsString();
  if (record_name == "") { return true; }

  clang::SourceLocation loc = RecordDecl->getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);
  const std::string     file_path = get_canonical_abs_path(file_name.str());

  if (file_path.empty()) {
    // Skip if the file path is empty
    return true;
  }

  if (is_system_file(file_path)) {
    // Skip system files
    return true;
  }

  ensure_file_key(output_json_, file_path);
  collect_disabled_macros(output_json_, file_path);

  // get record source code
  clang::SourceLocation start_loc = RecordDecl->getBeginLoc();
  clang::SourceLocation end_loc = RecordDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const std::string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();
  const int32_t start_line_no = src_manager_.getSpellingLineNumber(start_loc);
  const int32_t end_line_no = src_manager_.getSpellingLineNumber(end_loc);

  Json::Value &file_entry = output_json_[file_path];
  Json::Value &records_entry = file_entry["types"];

  ensure_key(records_entry, record_name);

  Json::Value &record_info = records_entry[record_name];
  record_info["definition"] = src_code;
  record_info["start_line"] = start_line_no;
  record_info["end_line"] = end_line_no;

  return true;
}

bool CodeDataVisitor::VisitEnumDecl(clang::EnumDecl *EnumDecl) {
  const std::string enum_name = EnumDecl->getNameAsString();
  if (enum_name == "") { return true; }

  clang::SourceLocation loc = EnumDecl->getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);
  const std::string     file_path = get_canonical_abs_path(file_name.str());

  if (file_path.empty()) {
    // Skip if the file path is empty
    return true;
  }

  if (is_system_file(file_path)) {
    // Skip system files
    return true;
  }

  ensure_file_key(output_json_, file_path);
  collect_disabled_macros(output_json_, file_path);

  // get enum source code
  clang::SourceLocation start_loc = EnumDecl->getBeginLoc();
  clang::SourceLocation end_loc = EnumDecl->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const std::string src_code =
      clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(range),
                                  src_manager_, lang_opts_)
          .str();
  const int32_t start_line_no = src_manager_.getSpellingLineNumber(start_loc);
  const int32_t end_line_no = src_manager_.getSpellingLineNumber(end_loc);

  Json::Value &file_entry = output_json_[file_path];
  Json::Value &enums_entry = file_entry["enums"];

  ensure_key(enums_entry, enum_name);

  Json::Value &enum_info = enums_entry[enum_name];
  enum_info["definition"] = src_code;
  enum_info["start_line"] = start_line_no;
  enum_info["end_line"] = end_line_no;

  return true;
}

/// ////////////////////////
// CodeDataASTConsumer class
// ////////////////////////
void CodeDataASTConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
}

// ////////////////////////
// CodeDataFrontendAction class
// ////////////////////////
std::unique_ptr<clang::ASTConsumer> CodeDataFrontendAction::CreateASTConsumer(
    clang::CompilerInstance &CI, llvm::StringRef InFile) {
  clang::SourceManager &source_manager = CI.getSourceManager();
  clang::LangOptions   &lang_opts = CI.getLangOpts();

  return std::make_unique<CodeDataASTConsumer>(source_manager, lang_opts,
                                               output_json_);
}

void CodeDataFrontendAction::ExecuteAction() {
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
  const std::string file_path = get_canonical_abs_path(file_name.str());

  if (is_system_file(file_path)) { return; }

  ensure_file_key(output_json_, file_path);
  collect_disabled_macros(output_json_, file_path);

  const std::string macro_name =
      MacroNameTok.getIdentifierInfo()->getName().str();

  clang::SourceLocation DefBegin = MI->getDefinitionLoc();
  clang::SourceLocation DefEnd = MI->getDefinitionEndLoc();

  const std::string def =
      "#define " + clang::Lexer::getSourceText(
                       clang::CharSourceRange::getTokenRange(DefBegin, DefEnd),
                       src_manager_, lang_opts_)
                       .str();
  const int32_t start_line_no = src_manager_.getSpellingLineNumber(DefBegin);
  const int32_t end_line_no = src_manager_.getSpellingLineNumber(DefEnd);

  Json::Value &file_entry = output_json_[file_path];
  Json::Value &macros_entry = file_entry["macros"];

  ensure_key(macros_entry, macro_name);
  Json::Value &macro_info = macros_entry[macro_name];

  macro_info["definition"] = def;
  macro_info["start_line"] = start_line_no;
  macro_info["end_line"] = end_line_no;
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

  PP.addPPCallbacks(
      std::make_unique<MacroPrinter>(SM, lang_opts, output_json_));

  PP.EnterMainSourceFile();
  clang::Token Tok;

  do {
    PP.Lex(Tok);
  } while (Tok.isNot(clang::tok::eof));

  remove_enabled_macros();

  return;
}

void MacroAction::remove_enabled_macros() {
  for (const std::string &file_name : output_json_.getMemberNames()) {
    Json::Value &enabled_macros = output_json_[file_name]["macros"];
    Json::Value &disabled_macros = output_json_[file_name]["disabled_macros"];

    for (const std::string &macro_name : disabled_macros.getMemberNames()) {
      if (!enabled_macros.isMember(macro_name)) { continue; }
      const std::string &enabled_def =
          enabled_macros[macro_name]["definition"].asString();

      Json::Value &disabled_defs = disabled_macros[macro_name];

      Json::Value remained = Json::Value(Json::arrayValue);

      for (const Json::Value &disabled_def : disabled_defs) {
        const std::string disabled_str = disabled_def["definition"].asString();

        if (disabled_str == enabled_def) { continue; }
        remained.append(disabled_def);
      }

      disabled_macros[macro_name] = remained;
    }

    Json::Value remained = Json::Value(Json::objectValue);

    for (const std::string &macro_name : disabled_macros.getMemberNames()) {
      Json::Value &disabled_defs = disabled_macros[macro_name];
      if (disabled_defs.size() == 0) { continue; }
      remained[macro_name] = disabled_defs;
    }

    output_json_[file_name]["disabled_macros"] = remained;
  }
  return;
}

// ////////////////////////
// // main function
// ////////////////////////

static std::vector<CompileCommand> read_compile_commands(
    const char *compile_commands_path) {
  static const std::set<std::string> src_extensions = {".c", ".cc", ".cpp",
                                                       ".cxx"};

  std::vector<CompileCommand> commands{};

  std::ifstream cc_file(compile_commands_path);

  if (!cc_file.is_open()) {
    std::cerr << "Error: could not open compile commands file "
              << compile_commands_path << "\n";
    return {};
  }

  std::string line;
  while (std::getline(cc_file, line)) {
    if (line.empty()) { continue; }

    size_t pos = line.find("-c");
    if (pos == std::string::npos) { continue; }

    pos = line.find_first_of(' ');
    if (pos == std::string::npos) { continue; }

    std::string working_dir = line.substr(0, pos);

    if (working_dir.find("TryCompile") != std::string::npos) { continue; }

    std::string command = line.substr(pos + 1);

    std::vector<std::string> commands_vec = tokenize_command(command);
    if (commands_vec.size() == 0) { continue; }

    size_t       src_index = -1;
    const size_t num_tokens = commands_vec.size();
    for (size_t index = 0; index < num_tokens; index++) {
      const std::string &token = commands_vec[index];
      for (const std::string &ext : src_extensions) {
        if (ends_with(token, ext)) {
          src_index = index;
          break;
        }
      }
      if (src_index != -1) { break; }
    }

    if (src_index == -1) { continue; }

    std::string src_path = commands_vec[src_index];

    if (src_path.find("conftest") != std::string::npos) { continue; }
    if (src_path.find("CMakeC") != std::string::npos) { continue; }

    bool skip = false;
    for (const std::string &excl : excludes) {
      if (src_path.find(excl) != std::string::npos) {
        skip = true;
        break;
      }
    }

    if (skip) { continue; }

    commands_vec.erase(commands_vec.begin() + src_index);

    add_system_include_paths(commands_vec);

    if (src_path[0] != '/') { src_path = working_dir + "/" + src_path; }

    CompileCommand compile_command(working_dir, commands_vec, src_path);
    commands.push_back(compile_command);
  }

  return commands;
}

static void write_output(const char        *output_filename,
                         const Json::Value &output_json) {
  std::ofstream output_file(output_filename);
  if (!output_file.is_open()) {
    std::cerr << "Error: could not open output file " << output_filename
              << "\n";
    return;
  }

  output_file << output_json.toStyledString();
  output_file.close();
  std::cout << "Wrote code data to " << output_filename << "\n";
  std::cout << "Total files found: " << output_json.size() << "\n";
  return;
}

int32_t main(int32_t argc, const char **argv) {
  if (argc < 3) {
    std::cout << "Usage: " << argv[0] << " <compile_commands.txt> <out.json>\n";
    std::cout << "  It takes one environment variable:\n";
    std::cout << "    EXCLUDES: A space-separated list of path fragments to"
              << " exclude from processing.\n";
    return 1;
  }

  const char *compile_commands_path = argv[1];
  const char *output_filename = argv[2];

  get_excludes();

  std::vector<CompileCommand> commands =
      read_compile_commands(compile_commands_path);

  if (commands.empty()) {
    std::cerr << "Error: No valid compile commands found.\n";
    return 1;
  }

  Json::Value output_json;

  for (const CompileCommand &cmd : commands) {
    const std::string              &src_path = cmd.src_file_;
    const std::vector<std::string> &compile_args = cmd.command_;
    std::ifstream                   src_file(src_path);
    if (!src_file.is_open()) {
      std::cerr << "Failed to open source file: " << src_path << "\n";
      continue;
    }

    std::stringstream src_buffer;
    src_buffer << src_file.rdbuf();
    src_file.close();

    const std::string src_code = src_buffer.str();

    clang::tooling::runToolOnCodeWithArgs(
        std::make_unique<CodeDataFrontendAction>(output_json), src_code,
        compile_args, src_path);

    clang::tooling::runToolOnCodeWithArgs(
        std::make_unique<MacroAction>(output_json), src_code, compile_args,
        src_path);
  }

  write_output(output_filename, output_json);
  return 0;
}
