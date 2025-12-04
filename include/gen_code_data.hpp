#ifndef GEN_CODE_DATA_HPP
#define GEN_CODE_DATA_HPP

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"
#include "jsoncpp/json/json.h"

class CodeDataVisitor : public clang::RecursiveASTVisitor<CodeDataVisitor> {
 public:
  explicit CodeDataVisitor(clang::SourceManager &src_manager,
                           clang::LangOptions   &lang_opts,
                           Json::Value          &output_json)
      : src_manager_(src_manager),
        lang_opts_(lang_opts),
        output_json_(output_json) {
  }

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

class CodeDataASTConsumer : public clang::ASTConsumer {
 public:
  explicit CodeDataASTConsumer(clang::SourceManager &src_manager,
                               clang::LangOptions   &lang_opts,
                               Json::Value          &output_json)
      : Visitor(src_manager, lang_opts, output_json) {
  }

  void HandleTranslationUnit(clang::ASTContext &Context) override;

 private:
  CodeDataVisitor Visitor;
};

class CodeDataFrontendAction : public clang::ASTFrontendAction {
 public:
  CodeDataFrontendAction(Json::Value &output_json)
      : output_json_(output_json) {};

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
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
  void remove_enabled_macros();

  Json::Value &output_json_;
};

#endif