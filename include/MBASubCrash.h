//========================================================================
// FILE:
//    MBASubCrash.h
//
// DESCRIPTION:
//    Demonstrates Iterator Invalidation crash.
//
// License: MIT
//========================================================================
#ifndef LLVM_TUTOR_MBA_SUB_H
#define LLVM_TUTOR_MBA_SUB_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

// PassInfoMixIn is a CRTP mix-in to automatically provide informational APIs
// needed for passes. Currently it provides only the 'name' method.
struct MBASubCrash : public llvm::PassInfoMixin<MBASubCrash> {
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &);
  bool runOnBasicBlock(llvm::BasicBlock &B);

  // Without isRequired returning true, this pass will be skipped for functions
  // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
  // all functions with optnone.
  static bool isRequired() { return true; }
};
#endif
