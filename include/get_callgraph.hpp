#ifndef GET_CALLGRAPH_HPP
#define GET_CALLGRAPH_HPP
#include <jsoncpp/json/json.h>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/CallGraph.h"
#include "clang/Frontend/FrontendAction.h"

using namespace std;

class CallgraphVisitor : public clang::RecursiveASTVisitor<CallgraphVisitor> {
 public:
  explicit CallgraphVisitor(clang::SourceManager &src_manager,
                            clang::LangOptions   &lang_opts,
                            llvm::StringRef src_path, Json::Value &output_json,
                            clang::CallGraph &CG);

  bool VisitFunctionDecl(clang::FunctionDecl *FuncDecl);

  void add_callee(const string &caller, const string &callee);

 private:
  clang::SourceManager &src_manager_;
  clang::LangOptions   &lang_opts_;
  llvm::StringRef       src_path_;
  Json::Value          &output_json_;
  clang::CallGraph     &CG_;
};

class CallgraphASTConsumer : public clang::ASTConsumer {
 public:
  explicit CallgraphASTConsumer(clang::SourceManager &src_manager,
                                clang::LangOptions   &lang_opts,
                                llvm::StringRef       src_path,
                                Json::Value          &output_json);

  void HandleTranslationUnit(clang::ASTContext &Context) override;

 private:
  clang::CallGraph CG_;
  CallgraphVisitor Visitor;
};

class CallgraphFrontendAction : public clang::ASTFrontendAction {
 public:
  CallgraphFrontendAction(Json::Value &output_json);

  unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &CI, llvm::StringRef InFile) override;

  void ExecuteAction() override;

 private:
  Json::Value &output_json_;
};

bool json_has_val_in_array(const Json::Value &json, const string &val);

#endif