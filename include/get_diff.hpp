#ifndef GET_DIFF_HPP
#define GET_DIFF_HPP

#include "clang/Tooling/ASTDiff/ASTDiff.h"

bool get_diff(clang::diff::SyntaxTree &ST1, clang::diff::SyntaxTree &ST2);

#endif