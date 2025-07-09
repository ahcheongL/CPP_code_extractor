#include "get_callgraph.hpp"

#include <fstream>
#include <iostream>

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "cpp_code_extractor_util.hpp"

// /////////////////////////
// CallgraphVisitor class
// /////////////////////////

CallgraphVisitor::CallgraphVisitor(clang::SourceManager &src_manager,
                                   clang::LangOptions   &lang_opts,
                                   llvm::StringRef       src_path,
                                   Json::Value          &output_json,
                                   clang::CallGraph     &CG)
    : src_manager_(src_manager),
      lang_opts_(lang_opts),
      src_path_(src_path),
      output_json_(output_json),
      CG_(CG) {
}

bool CallgraphVisitor::VisitFunctionDecl(clang::FunctionDecl *FuncDecl) {
  if (!FuncDecl->isThisDeclarationADefinition()) { return true; }
  string func_name = FuncDecl->getNameInfo().getName().getAsString();

  clang::SourceLocation loc = FuncDecl->getLocation();
  llvm::StringRef       file_name = src_manager_.getFilename(loc);

  if (file_name != src_path_) { return true; }

  clang::CallGraphNode *node = CG_.getOrInsertNode(FuncDecl);
  if (node == NULL) { return true; }

  for (clang::CallGraphNode::CallRecord callee : node->callees()) {
    clang::Expr *callee_expr = callee.CallExpr;
    if (callee_expr == nullptr) { continue; }

    // Callexpr
    if (clang::isa<clang::CallExpr>(callee_expr)) {
      clang::CallExpr *call_expr = clang::cast<clang::CallExpr>(callee_expr);
      clang::FunctionDecl *callee_func =
          call_expr->getDirectCallee();  // Get the callee function

      if (callee_func == nullptr) {
        llvm::outs() << "Skip indirect call expression : ";
        callee_expr->printPretty(llvm::outs(), nullptr, lang_opts_);
        llvm::outs() << "\n";
        continue;
      }

      const string callee_name =
          callee_func->getNameInfo().getName().getAsString();

      if (callee_name.empty()) {
        llvm::outs() << "Skip empty callee name in call expression : ";
        callee_expr->printPretty(llvm::outs(), nullptr, lang_opts_);
        llvm::outs() << "\n";
        continue;
      }

      add_callee(func_name, callee_name);
      continue;
    }

    // Binary operator
    if (clang::isa<clang::BinaryOperator>(callee_expr)) {
      clang::BinaryOperator *bin_op =
          clang::cast<clang::BinaryOperator>(callee_expr);

      clang::BinaryOperatorKind bin_op_kind = bin_op->getOpcode();

      llvm::outs() << "Skip binary operator in call expression : "
                   << clang::BinaryOperator::getOpcodeStr(bin_op_kind)
                   << " in function: " << func_name << ", Callee: ";
      callee_expr->printPretty(llvm::outs(), nullptr, lang_opts_);
      llvm::outs() << "\n";
      continue;
    }

    // CXXConstructExpr
    if (clang::isa<clang::CXXConstructExpr>(callee_expr)) {
      clang::CXXConstructExpr *cxx_construct_expr =
          clang::cast<clang::CXXConstructExpr>(callee_expr);

      const clang::CXXConstructorDecl *ctor_decl =
          cxx_construct_expr->getConstructor();

      if (ctor_decl == nullptr) {
        llvm::outs() << "Skip null constructor in call expression : ";
        callee_expr->printPretty(llvm::outs(), nullptr, lang_opts_);
        llvm::outs() << "\n";
        continue;
      }

      const string callee_name =
          ctor_decl->getNameInfo().getName().getAsString();

      if (callee_name.empty()) {
        llvm::outs() << "Skip empty callee name in CXXConstructExpr : ";
        callee_expr->printPretty(llvm::outs(), nullptr, lang_opts_);
        llvm::outs() << "\n";
        continue;
      }

      add_callee(func_name, callee_name);
      continue;
    }

    // CXXMemberCallExpr
    if (clang::isa<clang::CXXMemberCallExpr>(callee_expr)) {
      clang::CXXMemberCallExpr *member_call_expr =
          clang::cast<clang::CXXMemberCallExpr>(callee_expr);
      clang::Expr *implicit_obj_arg =
          member_call_expr->getImplicitObjectArgument();
      if (implicit_obj_arg == nullptr) {
        llvm::outs()
            << "Skip null implicit object argument in CXXMemberCallExpr :";
        callee_expr->printPretty(llvm::outs(), nullptr, lang_opts_);
        llvm::outs() << "\n";
        continue;
      }
      clang::CXXMethodDecl *method_decl =
          member_call_expr->getMethodDecl();  // Get the method declaration
      if (method_decl == nullptr) {
        llvm::outs() << "Skip null method declaration in CXXMemberCallExpr : ";
        callee_expr->printPretty(llvm::outs(), nullptr, lang_opts_);
        llvm::outs() << "\n";
        continue;
      }
      const string callee_name =
          method_decl->getNameInfo().getName().getAsString();
      if (callee_name.empty()) {
        llvm::outs() << "Skip empty callee name in CXXMemberCallExpr :";
        callee_expr->printPretty(llvm::outs(), nullptr, lang_opts_);
        llvm::outs() << "\n";
        continue;
      }
      add_callee(func_name, callee_name);
      continue;
    }

    llvm::outs() << "else case : Function: " << func_name << ", Callee: ";
    callee_expr->printPretty(llvm::outs(), nullptr, lang_opts_);
    llvm::outs() << "\n";
  }

  return true;
}

void CallgraphVisitor::add_callee(const string &caller, const string &callee) {
  if (!output_json_.isMember(caller)) {
    output_json_[caller] = Json::Value(Json::objectValue);
    output_json_[caller]["callees"] = Json::Value(Json::arrayValue);
    output_json_[caller]["callers"] = Json::Value(Json::arrayValue);
  }

  Json::Value &callees = output_json_[caller]["callees"];

  if (!json_has_val_in_array(callees, callee)) { callees.append(callee); }

  // Add the caller to the callee's caller list
  if (!output_json_.isMember(callee)) {
    output_json_[callee] = Json::Value(Json::objectValue);
    output_json_[callee]["callees"] = Json::Value(Json::arrayValue);
    output_json_[callee]["callers"] = Json::Value(Json::arrayValue);
  }

  Json::Value &callee_callers = output_json_[callee]["callers"];
  if (!json_has_val_in_array(callee_callers, caller)) {
    callee_callers.append(caller);
  }
  return;
}

// /////////////////////////
// CallgraphConsumer class
// /////////////////////////

CallgraphASTConsumer::CallgraphASTConsumer(clang::SourceManager &src_manager,
                                           clang::LangOptions   &lang_opts,
                                           llvm::StringRef       src_path,
                                           Json::Value          &output_json)
    : CG_(), Visitor(src_manager, lang_opts, src_path, output_json, CG_) {
}

void CallgraphASTConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
  clang::TranslationUnitDecl *tu_decl = Context.getTranslationUnitDecl();
  CG_.addToCallGraph(tu_decl);
  Visitor.TraverseDecl(tu_decl);
}

// /////////////////////////
// CallgraphFrontendAction class
// /////////////////////////

CallgraphFrontendAction::CallgraphFrontendAction(Json::Value &output_json)
    : output_json_(output_json) {
}

std::unique_ptr<clang::ASTConsumer> CallgraphFrontendAction::CreateASTConsumer(
    clang::CompilerInstance &CI, llvm::StringRef InFile) {
  auto &src_manager = CI.getSourceManager();
  auto &lang_opts = CI.getLangOpts();

  const clang::FileID main_file_id = src_manager.getMainFileID();

  clang::OptionalFileEntryRef main_file_ref =
      src_manager.getFileEntryRefForID(main_file_id);

  if (!main_file_ref) {
    llvm::errs() << "Error: Main file entry not found for ID: "
                 << main_file_id.getHashValue() << "\n";
    return nullptr;
  }

  llvm::StringRef main_file_name = main_file_ref->getName();

  return std::make_unique<CallgraphASTConsumer>(src_manager, lang_opts,
                                                main_file_name, output_json_);
}

void CallgraphFrontendAction::ExecuteAction() {
  clang::ASTFrontendAction::ExecuteAction();
  return;
}

// /////////////////////////
// main function
// /////////////////////////

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
      make_unique<CallgraphFrontendAction>(output_json), src_buffer.str(),
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

bool json_has_val_in_array(const Json::Value &json, const string &val) {
  if (!json.isArray()) { return false; }
  for (const auto &item : json) {
    if (item.asString() == val) { return true; }
  }
  return false;
}