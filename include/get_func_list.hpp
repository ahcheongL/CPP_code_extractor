#ifndef GET_FUNC_LIST_HPP
#define GET_FUNC_LIST_HPP

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"

class FunctionVisitor : public clang::RecursiveASTVisitor<FunctionVisitor> {
 public:
  explicit FunctionVisitor(clang::SourceManager &src_manager,
                           llvm::StringRef       src_path);

  bool VisitFunctionDecl(clang::FunctionDecl *FuncDecl);

 private:
  clang::SourceManager &src_manager_;
  llvm::StringRef       src_path_;
};

class FunctionASTConsumer : public clang::ASTConsumer {
 public:
  explicit FunctionASTConsumer(clang::SourceManager &src_manager,
                               llvm::StringRef       src_path);

  void HandleTranslationUnit(clang::ASTContext &Context) override;

 private:
  FunctionVisitor Visitor;
};

class FunctionFrontendAction : public clang::ASTFrontendAction {
 public:
  FunctionFrontendAction();

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &CI, llvm::StringRef InFile) override;

  void ExecuteAction() override;

 private:
};

#endif