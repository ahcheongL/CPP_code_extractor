#include "get_diff.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/Utils.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Sema/Sema.h"
#include "clang/Tooling/ASTDiff/ASTDiff.h"
#include "cpp_code_extractor_util.hpp"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/TargetParser/Host.h"

class DummyConsumer : public clang::ASTConsumer {
 public:
  void HandleTranslationUnit(clang::ASTContext &Context) override {
  }
};

std::unique_ptr<clang::CompilerInstance> createCompilerInstance(
    const std::string &filename, const std::vector<std::string> &args) {
  clang::IntrusiveRefCntPtr<clang::DiagnosticOptions> diagOpts =
      new clang::DiagnosticOptions();

  auto diagPrinter =
      new clang::TextDiagnosticPrinter(llvm::nulls(), diagOpts.get());

  clang::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs =
      llvm::vfs::getRealFileSystem();

  clang::IntrusiveRefCntPtr<clang::DiagnosticsEngine> diags =
      clang::CompilerInstance::createDiagnostics(*vfs, diagOpts.get(),
                                                 diagPrinter);

  std::vector<const char *> cstr_args{};
  for (const auto &arg : args) {
    cstr_args.push_back(arg.c_str());
  }
  cstr_args.push_back(filename.c_str());

  // Create compiler invocation from args
  auto invocation = std::make_shared<clang::CompilerInvocation>();
  bool success = clang::CompilerInvocation::CreateFromArgs(
      *invocation, cstr_args, *diags, "clang");

  if (!success) {
    llvm::errs()
        << "Error: Failed to create CompilerInvocation from arguments.\n";
    return nullptr;
  }

  auto CI = std::make_unique<clang::CompilerInstance>();
  CI->setInvocation(invocation);
  CI->setDiagnostics(diags.get());

  CI->createFileManager();
  clang::FileManager &file_manager = CI->getFileManager();
  CI->createSourceManager(file_manager);
  clang::SourceManager &source_manager = CI->getSourceManager();

  auto file_entry_expected = file_manager.getFileRef(filename);
  if (!file_entry_expected) {
    llvm::errs() << "Error: Could not open source file '" << filename << "\n";
    return nullptr;
  }

  clang::FileEntryRef file_entry = file_entry_expected.get();
  clang::FileID       main_file_id =
      source_manager.createFileID(file_entry, clang::SourceLocation(),
                                  clang::SrcMgr::CharacteristicKind::C_User);

  source_manager.setMainFileID(main_file_id);

  std::shared_ptr<clang::TargetOptions> target_options =
      std::make_shared<clang::TargetOptions>();
  target_options->Triple = llvm::sys::getDefaultTargetTriple();
  clang::TargetInfo *target_info =
      clang::TargetInfo::CreateTargetInfo(*diags, target_options);
  CI->setTarget(target_info);

  CI->createPreprocessor(clang::TU_Complete);
  CI->createASTContext();

  clang::Preprocessor &preprocessor = CI->getPreprocessor();

  CI->getDiagnosticClient().BeginSourceFile(CI->getLangOpts(), &preprocessor);

  DummyConsumer *consumer = new DummyConsumer();

  clang::ParseAST(preprocessor, consumer, CI->getASTContext());

  return CI;
}

bool get_diff(clang::diff::SyntaxTree &ST1, clang::diff::SyntaxTree &ST2) {
  if (ST1.getSize() != ST2.getSize()) { return true; }

  clang::diff::ASTDiff diff(ST1, ST2, clang::diff::ComparisonOptions());

  for (clang::diff::NodeId id1 : ST1) {
    clang::diff::NodeId id2 = diff.getMapped(ST1, id1);
    if (id2 == clang::diff::NodeId()) {
      llvm::outs() << "No mapping found for node ID: " << id1.Id << "\n";
      continue;  // No mapping found
    }

    const clang::diff::Node &node1 = ST1.getNode(id1);
    const clang::diff::Node &node2 = ST2.getNode(id2);

    if (node1.Change != clang::diff::ChangeKind::None ||
        node2.Change != clang::diff::ChangeKind::None) {
      return true;
    }
  }

  return false;
}

int main(int argc, const char **argv) {
  if (argc < 4) {
    std::cerr
        << "Usage: " << argv[0]
        << " <source-file1> <source-file2> <output.txt> -- [<compile args> "
           "...]\n";
    std::cerr << "Assumes both source files uses the same "
                 "compile arguments.\n";
    return 1;
  }

  const std::string src_path1 = argv[1];
  const std::string src_path2 = argv[2];
  const char       *output_filename = argv[3];

  const std::vector<std::string> compile_args = get_compile_args(argc, argv);

  auto CI1 = createCompilerInstance(src_path1, compile_args);
  auto CI2 = createCompilerInstance(src_path2, compile_args);

  clang::ASTContext &ASTContext1 = CI1->getASTContext();
  clang::ASTContext &ASTContext2 = CI2->getASTContext();

  clang::diff::SyntaxTree ST1(ASTContext1);
  clang::diff::SyntaxTree ST2(ASTContext2);

  const bool has_diff = get_diff(ST1, ST2);

  std::ofstream output_file(output_filename);
  if (!output_file.is_open()) {
    std::cerr << "Error: could not open output file " << output_filename
              << "\n";
    return 1;
  }

  if (has_diff) {
    output_file << "True";
  } else {
    output_file << "False";
  }

  output_file.close();

  llvm::outs() << "Output written to " << output_filename << "\n";
  llvm::outs() << "Differences found: " << (has_diff ? "Yes" : "No") << "\n";

  return 0;
}