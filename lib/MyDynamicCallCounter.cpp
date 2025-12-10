#include "MyDynamicCallCounter.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

#define DEBUG_TYPE "my-dynamic-cc"

Constant *CreateGlobalCounter(Module &M, StringRef GlobalVarName) {
  auto &CTX = M.getContext();

  // This will insert a declaration into M
  Constant *NewGlobalVar =
      M.getOrInsertGlobal(GlobalVarName, IntegerType::getInt32Ty(CTX));

  // This will change the declaration into definition (and initialise to 0)
  GlobalVariable *NewGV = M.getNamedGlobal(GlobalVarName);
  NewGV->setLinkage(GlobalValue::CommonLinkage);
  NewGV->setAlignment(MaybeAlign(4));
  NewGV->setInitializer(llvm::ConstantInt::get(CTX, APInt(32, 0)));

  return NewGlobalVar;
}


bool MyDynamicCallCounter::runOnModule(Module &M)
{
    bool Instrumented = false;

  // Function name <--> IR variable that holds the call counter
  llvm::StringMap<Constant *> CallCounterMap;
  // Function name <--> IR variable that holds the function name
  llvm::StringMap<Constant *> FuncNameMap;
  // Function args number map
  llvm::StringMap<Constant *> FuncArgNumMap;

  auto &CTX = M.getContext();

  // STEP 1: For each function in the module, inject a call-counting code
  // --------------------------------------------------------------------
  for (auto &F : M) {
    if (F.isDeclaration())
      continue;

    // Get an IR builder. Sets the insertion point to the top of the function
    IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());

    // Create a global variable to count the calls to this function
    std::string CounterName = "CounterFor_" + std::string(F.getName());
    //debug:97 Constant *Var = CreateGlobalCounter(M, CounterName);
    //(gdb) p CounterName
    //$1 = "CounterFor_foo"
    std::string ArgNumName= "ArgNumFor_" + std::string(F.getName());

    Constant *ArgsNumVar = CreateGlobalCounter(M, ArgNumName);
    Constant *Var = CreateGlobalCounter(M, CounterName);
    
    CallCounterMap[F.getName()] = Var;
    FuncArgNumMap[F.getName()] = ArgsNumVar;

    //get func args number for print func cuz we need to print func args number later
    uint32_t argNum = F.arg_size();
    Builder.CreateStore(Builder.getInt32(argNum), ArgsNumVar);

    // Create a global variable to hold the name of this function
    auto FuncName = Builder.CreateGlobalString(F.getName());
    FuncNameMap[F.getName()] = FuncName;
    //get func for print func cuz we need to print func name later

    // Inject instruction to increment the call count each time this function
    // executes
    LoadInst *Load2 = Builder.CreateLoad(IntegerType::getInt32Ty(CTX), Var);
    //%1 = load i32, ptr @CounterFor_foo, align 4
    Value *Inc2 = Builder.CreateAdd(Builder.getInt32(1), Load2);
    //%2 = add i32 1, %1
    Builder.CreateStore(Inc2, Var);
    //store i32 %2, ptr @CounterFor_foo, align 4

    // The following is visible only if you pass -debug on the command line
    // *and* you have an assert build.
    LLVM_DEBUG(dbgs() << " Instrumented: " << F.getName() << "\n");

    Instrumented = true;
  }

  // Stop here if there are no function definitions in this module
  if (false == Instrumented)
    return Instrumented;

  // STEP 2: Inject the declaration of printf
  // ----------------------------------------
  // Create (or _get_ in cases where it's already available) the following
  // declaration in the IR module:
  //    declare i32 @printf(i8*, ...)
  // It corresponds to the following C declaration:
  //    int printf(char *, ...)
  PointerType *PrintfArgTy = PointerType::getUnqual(Type::getInt8Ty(CTX));
  //char * 8bit ptr
  FunctionType *PrintfTy =
      FunctionType::get(IntegerType::getInt32Ty(CTX), PrintfArgTy,
                        /*IsVarArgs=*/true);
  //func signature  return int 32bit, first arg char* (8bit ptr), var args

  FunctionCallee Printf = M.getOrInsertFunction("printf", PrintfTy);
  //check if printf is already in the module, if not insert it with the above signature
  //we got something like this now:
  //int printf(const char *format, ...); c style
  //llvm ir:declare i32 @printf(i8*, ...)

  // Set attributes as per inferLibFuncAttributes in BuildLibCalls.cpp
  Function *PrintfF = dyn_cast<Function>(Printf.getCallee());
  PrintfF->setDoesNotThrow();
  PrintfF->addParamAttr(0, llvm::Attribute::getWithCaptureInfo(
                               M.getContext(), llvm::CaptureInfo::none()));
  //don't caputure ptr
  PrintfF->addParamAttr(0, Attribute::ReadOnly);
  //first arg is read only

  // STEP 3: Inject a global variable that will hold the printf format string
  // ------------------------------------------------------------------------
  llvm::Constant *ResultFormatStr =
      llvm::ConstantDataArray::getString(CTX, "%-20s %-10lu %d\n");
    //create a global string constant with the format we want to use to print results
  Constant *ResultFormatStrVar =
      M.getOrInsertGlobal("ResultFormatStrIR", ResultFormatStr->getType());
    //insert a global variable into the module to hold the format string
  dyn_cast<GlobalVariable>(ResultFormatStrVar)->setInitializer(ResultFormatStr);

  std::string out = "";
  out += "=================================================\n";
  out += "LLVM-TUTOR: mydynamic analysis results\n";
  out += "=================================================\n";
  out += "NAME     #N DIRECT CALLS     #N FUNC ARGS \n";
  out += "-------------------------------------------------\n";

  llvm::Constant *ResultHeaderStr =
      llvm::ConstantDataArray::getString(CTX, out.c_str());

  Constant *ResultHeaderStrVar =
      M.getOrInsertGlobal("ResultHeaderStrIR", ResultHeaderStr->getType());
  dyn_cast<GlobalVariable>(ResultHeaderStrVar)->setInitializer(ResultHeaderStr);

  // STEP 4: Define a printf wrapper that will print the results
  // -----------------------------------------------------------
  // Define `printf_wrapper` that will print the results stored in FuncNameMap
  // and CallCounterMap.  It is equivalent to the following C++ function:
  // ```
  //    void printf_wrapper() {
  //      for (auto &item : Functions)
  //        printf("llvm-tutor): Function %s was called %d times. \n",
  //        item.name, item.count);
  //    }
  // ```
  // (item.name comes from FuncNameMap, item.count comes from
  // CallCounterMap)
  FunctionType *PrintfWrapperTy =
      FunctionType::get(llvm::Type::getVoidTy(CTX), {},
                        /*IsVarArgs=*/false);
  //void printf_wrapper()
  Function *PrintfWrapperF = dyn_cast<Function>(
      M.getOrInsertFunction("printf_wrapper", PrintfWrapperTy).getCallee());
  //get or insert function into module

  // Create the entry basic block for printf_wrapper ...
  llvm::BasicBlock *RetBlock =
      llvm::BasicBlock::Create(CTX, "enter", PrintfWrapperF);
  IRBuilder<> Builder(RetBlock);
  //fill the basic block with instructions

  // ... and start inserting calls to printf
  // (printf requires i8*, so cast the input strings accordingly)
  llvm::Value *ResultHeaderStrPtr =
      Builder.CreatePointerCast(ResultHeaderStrVar, PrintfArgTy);
  llvm::Value *ResultFormatStrPtr =
      Builder.CreatePointerCast(ResultFormatStrVar, PrintfArgTy);
  //cast global vars to i8* (char*)
  //llvm::Constant *ResultFormatStr =
  // llvm::ConstantDataArray::getString(CTX, "%-20s %-10lu\n");
  // it is array not a pointer, so we need to cast it

  Builder.CreateCall(Printf, {ResultHeaderStrPtr});

  LoadInst *LoadCounter;
  LoadInst *LoadArgNum;
  for (auto &item : CallCounterMap) {
    LoadCounter = Builder.CreateLoad(IntegerType::getInt32Ty(CTX), item.second);
    // LoadCounter = Builder.CreateLoad(item.second);
    // This is the real function call counter
    Constant * ArgNumVar = FuncArgNumMap[item.first()];
    LoadArgNum = Builder.CreateLoad(IntegerType::getInt32Ty(CTX), ArgNumVar);
    // LoadArgNum = Builder.CreateLoad(ArgNumVar);
    Builder.CreateCall(
        Printf, {ResultFormatStrPtr, FuncNameMap[item.first()], LoadCounter, LoadArgNum});
  }

  // Finally, insert return instruction
  Builder.CreateRetVoid();

  // STEP 5: Call `printf_wrapper` at the very end of this module
  // ------------------------------------------------------------
  appendToGlobalDtors(M, PrintfWrapperF, /*Priority=*/0);

  return true;
}


PreservedAnalyses MyDynamicCallCounter::run(llvm::Module &M,
                                          llvm::ModuleAnalysisManager &) {
  bool Changed = runOnModule(M);

  return (Changed ? llvm::PreservedAnalyses::none()
                  : llvm::PreservedAnalyses::all());
}






//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getMyDynamicCallCounterPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "my-dynamic-cc", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "my-dynamic-cc") {
                    MPM.addPass(MyDynamicCallCounter());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getMyDynamicCallCounterPluginInfo();
}
