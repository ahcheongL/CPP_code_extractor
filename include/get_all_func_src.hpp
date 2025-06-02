#ifndef GET_FUNC_SRC_HPP
#define GET_FUNC_SRC_HPP
#include <jsoncpp/json/json.h>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"

using namespace std;

class AllFuncSrcVisitor : public clang::RecursiveASTVisitor<AllFuncSrcVisitor> {
 public:
  explicit AllFuncSrcVisitor(clang::SourceManager &src_manager,
                             clang::LangOptions   &lang_opts,
                             const char *src_path, Json::Value &output_json);

  bool VisitFunctionDecl(clang::FunctionDecl *FuncDecl);

 private:
  clang::SourceManager &src_manager_;
  clang::LangOptions   &lang_opts_;
  const char           *src_path_;
  Json::Value          &output_json_;
};

class AllFuncSrcASTConsumer : public clang::ASTConsumer {
 public:
  explicit AllFuncSrcASTConsumer(clang::SourceManager &src_manager,
                                 clang::LangOptions   &lang_opts,
                                 const char           *src_path,
                                 Json::Value          &output_json);

  void HandleTranslationUnit(clang::ASTContext &Context) override;

 private:
  AllFuncSrcVisitor Visitor;
};

class AllFuncSrcFrontendAction : public clang::ASTFrontendAction {
 public:
  AllFuncSrcFrontendAction(Json::Value &output_json);

  unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &CI, llvm::StringRef InFile) override;

  void ExecuteAction() override;

 private:
  Json::Value &output_json_;
};

#endif