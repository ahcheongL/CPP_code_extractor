#ifndef GET_SRC_RANGE_HPP
#define GET_SRC_RANGE_HPP

#include <jsoncpp/json/json.h>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"

using namespace std;

class SrcRangeVisitor : public clang::RecursiveASTVisitor<SrcRangeVisitor> {
 public:
  explicit SrcRangeVisitor(clang::SourceManager &src_manager,
                           clang::LangOptions   &lang_opts,
                           llvm::StringRef src_path, Json::Value &output_json);

  bool VisitFunctionDecl(clang::FunctionDecl *FuncDecl);

 private:
  clang::SourceManager &src_manager_;
  clang::LangOptions   &lang_opts_;
  llvm::StringRef       src_path_;
  Json::Value          &output_json_;
};

class SrcRangeASTConsumer : public clang::ASTConsumer {
 public:
  explicit SrcRangeASTConsumer(clang::SourceManager &src_manager,
                               clang::LangOptions   &lang_opts,
                               llvm::StringRef       src_path,
                               Json::Value          &output_json);

  void HandleTranslationUnit(clang::ASTContext &Context) override;

 private:
  SrcRangeVisitor Visitor;
};

class SrcRangeFrontendAction : public clang::ASTFrontendAction {
 public:
  SrcRangeFrontendAction(Json::Value &output_json);

  unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &CI, llvm::StringRef InFile) override;

  void ExecuteAction() override;

 private:
  Json::Value &output_json_;
};

#endif