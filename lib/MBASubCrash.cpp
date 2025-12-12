//==============================================================================
// FILE:
//    MBASub.cpp (BROKEN VERSION for educational purposes)
//
// DESCRIPTION:
//    Demonstrates Iterator Invalidation crash.
//==============================================================================

#include "MBASubCrash.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <random>

using namespace llvm;

#define DEBUG_TYPE "mba-sub-crash"

STATISTIC(SubstCount, "The # of substituted instructions");

//-----------------------------------------------------------------------------
// MBASub Implementaion
//-----------------------------------------------------------------------------
bool MBASubCrash::runOnBasicBlock(BasicBlock &BB) {
  bool Changed = false;

  // Loop over all instructions in the block. Replacing instructions requires
  // iterators, hence a for-range loop wouldn't be suitable.
  for (auto &Inst : BB)
  //Change (auto Inst = BB.begin(), IE = BB.end(); Inst != IE; ++Inst) into (auto &Inst : BB) to cause iterator invalidation
  {
    // Skip non-binary (e.g. unary or compare) instruction.
    auto *BinOp = dyn_cast<BinaryOperator>(&Inst);
    if (!BinOp)
      continue;

    unsigned Opcode = BinOp->getOpcode();
    if (Opcode != Instruction::Sub || !BinOp->getType()->isIntegerTy())
      continue;
    // A uniform API for creating instructions and inserting
    // them into basic blocks.
    IRBuilder<> Builder(BinOp);

    // Create an instruction representing (a + ~b) + 1
    // %7 = sub nsw i32 %5, %6 %5=getOperand(0), %6=getOperand(1)
    Instruction *NewValue = BinaryOperator::CreateAdd(
        Builder.CreateAdd(BinOp->getOperand(0),
                          Builder.CreateNot(BinOp->getOperand(1))//%7 = xor i32 %6, -1 (step 1)
                         ),//%8 = add i32 %5, %7  (step 2)
        ConstantInt::get(BinOp->getType(), 1)
                                                    );  //<badref> = add i32 %8, 1 (step 3)

    // The following is visible only if you pass -debug on the command line
    // *and* you have an assert build.
    LLVM_DEBUG(dbgs() << *BinOp << " -> " << *NewValue << "\n");

    // Replace `(a - b)` (original instructions) with `(a + ~b) + 1`
    // (the new instruction)
    ReplaceInstWithInst(&Inst, NewValue);
    Changed = true;

    // Update the statistics
    ++SubstCount;
  }

  return Changed;
}

PreservedAnalyses MBASubCrash::run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &) {
  bool Changed = false;

  for (auto &BB : F) {
    Changed |= runOnBasicBlock(BB);
  }
  return (Changed ? llvm::PreservedAnalyses::none()
                  : llvm::PreservedAnalyses::all());
}
//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getMBASubCrashPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "mba-sub-crash", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "mba-sub-crash") {
                    FPM.addPass(MBASubCrash());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getMBASubCrashPluginInfo();
}
