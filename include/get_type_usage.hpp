#ifndef GET_TYPE_USAGE_HPP
#define GET_TYPE_USAGE_HPP
#include <jsoncpp/json/json.h>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"

using namespace std;
// /////////////////////////
// TypeUsageVisitor
// /////////////////////////

class TypeUsageVisitor : public clang::RecursiveASTVisitor<TypeUsageVisitor> {
public:
  explicit TypeUsageVisitor(clang::SourceManager &src_manager,
                   clang::LangOptions   &lang_opts,
                   Json::Value          &output_json);

  bool VisitFunctionDecl(clang::FunctionDecl *FuncDecl);

private:
  clang::SourceManager &src_manager_;
  clang::LangOptions   &lang_opts_;
  Json::Value          &output_json_;
};

// /////////////////////////
// PerFunctionUsageVisitor
// /////////////////////////

class PerFunctionUsageVisitor: public clang::RecursiveASTVisitor<PerFunctionUsageVisitor> {
public:
  PerFunctionUsageVisitor(clang::SourceManager &src_manager,
                          clang::LangOptions   &lang_opts,
                          Json::Value          &output_json,
                          const std::string    &file_path,
                          const std::string    &func_name);

  bool VisitBinaryOperator(clang::BinaryOperator *BinOp);
  bool VisitUnaryOperator(clang::UnaryOperator *UO);

private:
  clang::SourceManager &src_manager_;
  clang::LangOptions   &lang_opts_;
  Json::Value          &output_json_;
  std::string file_path_;
  std::string func_name_;
};

// /////////////////////////
// TypeUsageASTConsumer
// /////////////////////////

class TypeUsageASTConsumer : public clang::ASTConsumer {
public:
  TypeUsageASTConsumer(clang::SourceManager &src_manager,
                       clang::LangOptions   &lang_opts,
                       Json::Value          &output_json);

  void HandleTranslationUnit(clang::ASTContext &Context) override;

private:
  TypeUsageVisitor Visitor;
};

// /////////////////////////
// TypeUsageFrontendAction
// /////////////////////////

class TypeUsageFrontendAction : public clang::ASTFrontendAction {
public:
  explicit TypeUsageFrontendAction(Json::Value &output_json);

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI,
                    llvm::StringRef InFile) override;

  void ExecuteAction() override;

private:
  Json::Value &output_json_;
};
#endif