#ifndef PARSE_CPP_HPP
#define PARSE_CPP_HPP
#include <jsoncpp/json/json.h>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"

class ParseASTConsumer : public clang::ASTConsumer {
 public:
  explicit ParseASTConsumer(clang::SourceManager &src_manager,
                            clang::LangOptions   &lang_opts,
                            llvm::StringRef src_path, Json::Value &output_json);

  void HandleTranslationUnit(clang::ASTContext &Context) override;

 private:
  clang::SourceManager &src_manager_;
  clang::LangOptions   &lang_opts_;
  llvm::StringRef       src_path_;
  Json::Value          &output_json_;
};

class ParseFrontendAction : public clang::ASTFrontendAction {
 public:
  ParseFrontendAction(Json::Value &output_json);

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &CI, llvm::StringRef InFile) override;

  void ExecuteAction() override;

 private:
  Json::Value &output_json_;
};

#endif