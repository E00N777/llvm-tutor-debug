//==============================================================================
// FILE:
//    MBAAddInt16.cpp
// AUTHOR:
//    @E0N
//
// DESCRIPTION:
//    This pass performs a substitution for 16-bit integer add
//    instruction based on this Mixed Boolean-Airthmetic expression:
//      a + b == (((a ^ b) + 2 * (a & b)) * 42601 + 49826) * 18905 + 53934
//    You can generate these magic number by scripts/Int16MagicNumGen.py
//
// USAGE:
//      $ opt -load-pass-plugin <BUILD_DIR>/lib/libMBAAddInt16.so `\`
//        -passes=-"mba-add-16" <bitcode-file>
//      The command line option is not available for the new PM
//
//  
// [1] "Defeating MBA-based Obfuscation" Ninon Eyrolles, Louis Goubin, Marion
//     Videau
//
// License: MIT
//==============================================================================
#include "MBAAddInt16.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"


using namespace llvm;

#define DEBUG_TYPE "mba-add-16"

STATISTIC(SubstCount, "The # of substituted instructions");


bool MBAAddInt16::runOnBasicBlock(BasicBlock &BB)
{
  bool Changed = false;

  for(auto Inst=BB.begin(), IE=BB.end(); Inst!=IE ;++Inst)
  {
      auto *BinOp = dyn_cast<BinaryOperator>(Inst);

      if (!BinOp)
      continue;

    // Skip instructions other than add
    if (BinOp->getOpcode() != Instruction::Add)
      continue;

    IRBuilder<> Builder(BinOp);

    auto Val42601=ConstantInt::get(BinOp->getType(), 42601);
    auto Val49826=ConstantInt::get(BinOp->getType(), 49826);
    auto Val18905=ConstantInt::get(BinOp->getType(), 18905);
    auto Val53934=ConstantInt::get(BinOp->getType(), 53934);
    auto Val2=ConstantInt::get(BinOp->getType(), 2);


        Instruction *NewInst =
        // E = e5 + 111
        BinaryOperator::CreateAdd(
            Val53934,
            // e5 = e4 * 151
            Builder.CreateMul(
                Val18905,
                // e4 = e2 + 23
                Builder.CreateAdd(
                    Val49826,
                    // e3 = e2 * 39
                    Builder.CreateMul(
                        Val42601,
                        // e2 = e0 + e1
                        Builder.CreateAdd(
                            // e0 = a ^ b
                            Builder.CreateXor(BinOp->getOperand(0),
                                              BinOp->getOperand(1)),//step 1
                            // e1 = 2 * (a & b)
                            Builder.CreateMul(
                                Val2, Builder.CreateAnd(BinOp->getOperand(0),
                                                        BinOp->getOperand(1)) //step 2
                                                      )
                                                      ) 
                    ) // e3 = e2 * 39
                ) // e4 = e2 + 23
            ) // e5 = e4 * 151
        ); // E = e5 + 111
    
  LLVM_DEBUG(dbgs() << *BinOp << " -> " << *NewInst << "\n");

    // Replace `(a + b)` (original instructions) with `(((a ^ b) + 2 * (a & b))
    // * 39 + 23) * 151 + 111` (the new instruction)
    ReplaceInstWithInst(&BB, Inst, NewInst);
    Changed = true;

    // Update the statistics
    ++SubstCount;


  }

    return Changed;
}



PreservedAnalyses MBAAddInt16::run(llvm::Function &F,
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
llvm::PassPluginLibraryInfo getMBAAddInt16PluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "mba-add-16", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "mba-add-16") {
                    FPM.addPass(MBAAddInt16());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getMBAAddInt16PluginInfo();
}
