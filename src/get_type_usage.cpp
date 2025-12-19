#include "get_type_usage.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"

#include "cpp_code_extractor_util.hpp"

using std::string;
using std::vector;


static void add_object(Json::Value *root,
                       const std::vector<std::string> &path,
                       const Json::Value &value) {
  Json::Value *current = root;

  for (size_t i = 0; i < path.size() - 1; i++) {
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
  Json::Value *current = root;

  for (size_t i = 0; i < path.size() - 1; i++) {
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

// ------------------------
// TypeUsageVisitor
// ------------------------

TypeUsageVisitor::TypeUsageVisitor(clang::SourceManager &src_manager,
                                   clang::LangOptions   &lang_opts,
                                   Json::Value          &output_json)
    : src_manager_(src_manager), lang_opts_(lang_opts),
      output_json_(output_json) {}

// This visitor only enters each function once,
// then runs PerFunctionUsageVisitor on the function body.
bool TypeUsageVisitor::VisitFunctionDecl(clang::FunctionDecl *FuncDecl) {
  if (!FuncDecl->isThisDeclarationADefinition()) {
    return true;
  }

  const string func_name =
      FuncDecl->getNameInfo().getName().getAsString();
  if (func_name.empty()) {
    return true;
  }

  clang::SourceLocation loc = FuncDecl->getLocation();
  llvm::StringRef file_name = src_manager_.getFilename(loc);
  const string file_path = get_abs_path(file_name.str());

  if (file_path.empty()) {
    return true;
  }
  if (is_system_file(file_path)) {
    return true;
  }

  // Collect per-function assignments and address usages.
  clang::Stmt *Body = FuncDecl->getBody();
  if (!Body) {
    return true;
  }

  PerFunctionUsageVisitor V(src_manager_, lang_opts_, output_json_,
                            file_path, func_name);
  V.TraverseStmt(Body);

  return true;
}

// ------------------------
// PerFunctionUsageVisitor
// ------------------------

PerFunctionUsageVisitor::PerFunctionUsageVisitor(
    clang::SourceManager &src_manager,
    clang::LangOptions   &lang_opts,
    Json::Value          &output_json,
    const std::string    &file_path,
    const std::string    &func_name)
    : src_manager_(src_manager),
      lang_opts_(lang_opts),
      output_json_(output_json),
      file_path_(file_path),
      func_name_(func_name) {}

bool PerFunctionUsageVisitor::VisitBinaryOperator(
    clang::BinaryOperator *BinOp) {
  if (!BinOp->isAssignmentOp()) {
    return true;
  }

  clang::Expr *lhs = BinOp->getLHS();
  clang::Expr *rhs = BinOp->getRHS();
  if (!lhs || !rhs) {
    return true;
  }

  // Extract source
  clang::SourceLocation start_loc = BinOp->getBeginLoc();
  clang::SourceLocation end_loc   = BinOp->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(
          clang::CharSourceRange::getTokenRange(range),
          src_manager_, lang_opts_).str();

  const int line_start_no = src_manager_.getSpellingLineNumber(start_loc);
  const int line_end_no   = src_manager_.getSpellingLineNumber(end_loc);

  // Types (as strings)
  const string lhs_ty =
      lhs->IgnoreParenImpCasts()->getType().getAsString();
  const string rhs_ty =
      rhs->IgnoreParenImpCasts()->getType().getAsString();

  Json::Value info(Json::objectValue);
  info["code"] = src_code;
  info["line_start"] = line_start_no;
  info["line_end"] = line_end_no;

  Json::Value lhs_types(Json::arrayValue);
  Json::Value rhs_types(Json::arrayValue);
  lhs_types.append(lhs_ty);
  rhs_types.append(rhs_ty);
  info["lhs_type"] = lhs_types;
  info["rhs_type"] = rhs_types;

  // Desired schema:
  // file -> functions -> func -> assignments -> [ ... ]
  add_list(&output_json_,
           {file_path_, "functions", func_name_, "assignments"},
           info);

  return true;
}

bool PerFunctionUsageVisitor::VisitUnaryOperator(clang::UnaryOperator *UO) {
  if (UO->getOpcode() != clang::UO_AddrOf) {
    return true;
  }

  const clang::Expr *sub = UO->getSubExpr();
  if (!sub) {
    return true;
  }

  const string obj_ty =
      sub->IgnoreParenImpCasts()->getType().getAsString();

  clang::SourceLocation start_loc = UO->getBeginLoc();
  clang::SourceLocation end_loc   = UO->getEndLoc();
  clang::SourceRange    range(start_loc, end_loc);

  const string src_code =
      clang::Lexer::getSourceText(
          clang::CharSourceRange::getTokenRange(range),
          src_manager_, lang_opts_).str();

  const int line_start_no = src_manager_.getSpellingLineNumber(start_loc);
  const int line_end_no   = src_manager_.getSpellingLineNumber(end_loc);

  Json::Value info(Json::objectValue);
  info["code"] = src_code;
  info["line_start"] = line_start_no;
  info["line_end"] = line_end_no;

  Json::Value obj_types(Json::arrayValue);
  obj_types.append(obj_ty);
  info["object_type"] = obj_types;

  // file -> functions -> func -> address_usages -> [ ... ]
  add_list(&output_json_,
           {file_path_, "functions", func_name_, "address_usages"},
           info);

  return true;
}

// ------------------------
// TypeUsageASTConsumer
// ------------------------

TypeUsageASTConsumer::TypeUsageASTConsumer(clang::SourceManager &src_manager,
                                           clang::LangOptions   &lang_opts,
                                           Json::Value          &output_json)
    : Visitor(src_manager, lang_opts, output_json) {}

void TypeUsageASTConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
}

// ------------------------
// TypeUsageFrontendAction
// ------------------------

TypeUsageFrontendAction::TypeUsageFrontendAction(Json::Value &output_json)
    : output_json_(output_json) {}

std::unique_ptr<clang::ASTConsumer>
TypeUsageFrontendAction::CreateASTConsumer(clang::CompilerInstance &CI,
                                           llvm::StringRef InFile) {
  clang::SourceManager &source_manager = CI.getSourceManager();
  clang::LangOptions   &lang_opts      = CI.getLangOpts();
  return std::make_unique<TypeUsageASTConsumer>(source_manager, lang_opts,
                                                output_json_);
}

void TypeUsageFrontendAction::ExecuteAction() {
  clang::ASTFrontendAction::ExecuteAction();
}

// ------------------------
// main
// ------------------------

int main(int argc, const char **argv) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <source-file> <out.json> -- [<compile args> ...]\n";
    return 1;
  }

  const string src_path = argv[1];
  const char *output_filename = argv[2];

  const vector<string> compile_args = get_compile_args(argc, argv);

  std::ifstream src_file(src_path);
  if (!src_file.is_open()) {
    std::cerr << "Error: could not open source file " << src_path << "\n";
    return 1;
  }

  std::stringstream src_buffer;
  src_buffer << src_file.rdbuf();
  src_file.close();

  Json::Value output_json;

  clang::tooling::runToolOnCodeWithArgs(
      std::make_unique<TypeUsageFrontendAction>(output_json),
      src_buffer.str(), compile_args, src_path);

  std::ofstream output_file(output_filename);
  if (!output_file.is_open()) {
    std::cerr << "Error: could not open output file " << output_filename
              << "\n";
    return 1;
  }

  output_file << output_json.toStyledString();
  output_file.close();

  std::cout << "Type usage extracted to " << output_filename << "\n";
  return 0;
}
