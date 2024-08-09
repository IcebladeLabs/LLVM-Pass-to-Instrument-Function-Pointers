//========================================================================
// FILE:
//    DynamicFP.cpp
//
// USAGE:
//      $ opt -load-pass-plugin <BUILD_DIR>/lib/libInjectFunctCall.so `\`
//        -passes=-"inject-func-call" <bitcode-file>
//
// License: MIT
//========================================================================
#include "InjectFuncCall.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "inject-func-call"

//-----------------------------------------------------------------------------
// InjectFuncCall implementation
//-----------------------------------------------------------------------------
bool DynamicFP::runOnModule(Module &M) {
  bool InsertedAtLeastOnePrintf = false;

  auto &CTX = M.getContext();
  PointerType *PrintfArgTy = PointerType::getUnqual(Type::getInt8Ty(CTX));

  // STEP 1: Inject the declaration of printf
  // ----------------------------------------
  // Create (or _get_ in cases where it's already available) the following
  // declaration in the IR module:
  //    declare i32 @printf(i8*, ...)
  // It corresponds to the following C declaration:
  //    int printf(char *, ...)
  FunctionType *PrintfTy = FunctionType::get(
      IntegerType::getInt32Ty(CTX),
      PrintfArgTy,
      /*IsVarArgs=*/true);

  FunctionCallee Printf = M.getOrInsertFunction("printf", PrintfTy);

  // Set attributes as per inferLibFuncAttributes in BuildLibCalls.cpp
  Function *PrintfF = dyn_cast<Function>(Printf.getCallee());
  PrintfF->setDoesNotThrow();
  PrintfF->addParamAttr(0, Attribute::NoCapture);
  PrintfF->addParamAttr(0, Attribute::ReadOnly);


  // STEP 2: Inject a global variable that will hold the printf format string
  // ------------------------------------------------------------------------
  llvm::Constant *PrintfFormatStr = llvm::ConstantDataArray::getString(
      CTX, "(fp-trance) Called from: %s\n(fp-trace)   Called: %i\n");

  Constant *PrintfFormatStrVar =
      M.getOrInsertGlobal("PrintfFormatStr", PrintfFormatStr->getType());
  dyn_cast<GlobalVariable>(PrintfFormatStrVar)->setInitializer(PrintfFormatStr);

  // STEP 3: For each function in the module, inject a call to printf
  // ----------------------------------------------------------------
  for (auto &F : M) {

    for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *Call = dyn_cast<CallBase>(&I)) {
                    if (auto *CalledFunction = Call->getCalledFunction()) {
                        // Direct function call, ignore.
                        continue;
                    } else {
                        Value *CalledValue = Call->getCalledOperand();
                        if (CalledValue->getType()->isPointerTy()) {
                            // Get an IR builder. Sets the insertion point to the top of the function
    			    IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());

    			    // Inject a global variable that contains the function name
    		 	    auto FuncName = Builder.CreateGlobalStringPtr(F.getName());
	
			    // Printf requires i8*, but PrintfFormatStrVar is an array: [n x i8]. Add
			    // a cast: [n x i8] -> i8*
			    llvm::Value *FormatStrPtr =
				Builder.CreatePointerCast(PrintfFormatStrVar, PrintfArgTy, "formatStr");

			    // The following is visible only if you pass -debug on the command line
			    // *and* you have an assert build.
			    LLVM_DEBUG(dbgs() << " Injecting call to printf inside " << F.getName()
					      << "\n");

			    // Finally, inject a call to printf
			    Builder.CreateCall(
				Printf, {FormatStrPtr, FuncName, CalledValue});

			    InsertedAtLeastOnePrintf = true;
                            
                        }
                    }
                }
            }
        }

    
  }

  return InsertedAtLeastOnePrintf;
}

PreservedAnalyses DynamicFP::run(llvm::Module &M,
                                       llvm::ModuleAnalysisManager &) {
  bool Changed =  runOnModule(M);

  return (Changed ? llvm::PreservedAnalyses::none()
                  : llvm::PreservedAnalyses::all());
}


//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getDynamicFPPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerPipelineEarlySimplificationEPCallback(
        [&](ModulePassManager &MPM, auto) {
          MPM.addPass(DynamicFP());
          return true;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "name", "0.0.1", callback};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getDynamicFPPluginInfo();
}
