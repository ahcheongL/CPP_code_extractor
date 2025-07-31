#ifndef GET_ALL_SRC_HPP
#define GET_ALL_SRC_HPP
#include <jsoncpp/json/json.h>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"

using namespace std;

class AllSrcVisitor : public clang::RecursiveASTVisitor<AllSrcVisitor> {
 public:
  explicit AllSrcVisitor(clang::SourceManager &src_manager,
                         clang::LangOptions   &lang_opts,
                         Json::Value          &output_json);

  bool VisitFunctionDecl(clang::FunctionDecl *FuncDecl);
  bool VisitVarDecl(clang::VarDecl *VarDecl);
  bool VisitTypedefDecl(clang::TypedefDecl *TypedefDecl);
  bool VisitRecordDecl(clang::RecordDecl *RecordDecl);
  bool VisitEnumDecl(clang::EnumDecl *EnumDecl);

 private:
  clang::SourceManager &src_manager_;
  clang::LangOptions   &lang_opts_;
  Json::Value          &output_json_;
};

class AllSrcASTConsumer : public clang::ASTConsumer {
 public:
  explicit AllSrcASTConsumer(clang::SourceManager &src_manager,
                             clang::LangOptions   &lang_opts,
                             Json::Value          &output_json);

  void HandleTranslationUnit(clang::ASTContext &Context) override;

 private:
  AllSrcVisitor Visitor;
};

class AllSrcFrontendAction : public clang::ASTFrontendAction {
 public:
  AllSrcFrontendAction(Json::Value &output_json);

  unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &CI, llvm::StringRef InFile) override;

  void ExecuteAction() override;

 private:
  Json::Value &output_json_;
};

class MacroPrinter : public clang::PPCallbacks {
 public:
  MacroPrinter(clang::SourceManager &SM, clang::LangOptions &LangOpts,
               Json::Value &output_json);

  void MacroDefined(const clang::Token          &MacroNameTok,
                    const clang::MacroDirective *MD) override;

 private:
  clang::SourceManager &src_manager_;
  clang::LangOptions   &lang_opts_;
  Json::Value          &output_json_;
};

class MacroAction : public clang::PreprocessorFrontendAction {
 public:
  MacroAction(Json::Value &output_json);

  void ExecuteAction() override;

 private:
  Json::Value &output_json_;
};

#endif