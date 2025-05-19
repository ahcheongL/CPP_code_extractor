#ifndef GET_FUNC_LIST_HPP
#define GET_FUNC_LIST_HPP

#include <set>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"

using namespace std;

class FunctionVisitor : public clang::RecursiveASTVisitor<FunctionVisitor> {
 public:
  explicit FunctionVisitor(clang::SourceManager &src_manager,
                           const char           *src_path);

  bool VisitFunctionDecl(clang::FunctionDecl *FuncDecl);

 private:
  clang::SourceManager &src_manager_;
  const char           *src_path_;
};

class FunctionASTConsumer : public clang::ASTConsumer {
 public:
  explicit FunctionASTConsumer(clang::SourceManager &src_manager,
                               const char           *src_path);

  void HandleTranslationUnit(clang::ASTContext &Context) override;

 private:
  FunctionVisitor Visitor;
};

class FunctionFrontendAction : public clang::ASTFrontendAction {
 public:
  FunctionFrontendAction(const char *src_path);

  unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &CI, llvm::StringRef InFile) override;

  void ExecuteAction() override;

 private:
  const char *src_path_;
};

#endif