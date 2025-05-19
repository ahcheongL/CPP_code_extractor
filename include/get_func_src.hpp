#ifndef GET_FUNC_SRC_HPP
#define GET_FUNC_SRC_HPP

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"

using namespace std;

class FuncSrcVisitor : public clang::RecursiveASTVisitor<FuncSrcVisitor> {
 public:
  explicit FuncSrcVisitor(clang::SourceManager &src_manager,
                          clang::LangOptions &lang_opts, const char *src_path,
                          const char *target_func);

  bool VisitFunctionDecl(clang::FunctionDecl *FuncDecl);

 private:
  clang::SourceManager &src_manager_;
  clang::LangOptions   &lang_opts_;
  const char           *src_path_;
  const char           *target_func_;
};

class FuncSrcASTConsumer : public clang::ASTConsumer {
 public:
  explicit FuncSrcASTConsumer(clang::SourceManager &src_manager,
                              clang::LangOptions   &lang_opts,
                              const char *src_path, const char *target_func);

  void HandleTranslationUnit(clang::ASTContext &Context) override;

 private:
  FuncSrcVisitor Visitor;
};

class FuncSrcFrontendAction : public clang::ASTFrontendAction {
 public:
  FuncSrcFrontendAction(const char *target_func);

  unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &CI, llvm::StringRef InFile) override;

  void ExecuteAction() override;

 private:
  const char *target_func_;
};

#endif