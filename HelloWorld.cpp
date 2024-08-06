//=============================================================================
// FILE:
//    HelloWorld.cpp
//
// DESCRIPTION:
//    Visits all functions in a module, prints their names and the number of
//    arguments via stderr. Strictly speaking, this is an analysis pass (i.e.
//    the functions are not modified). However, in order to keep things simple
//    there's no 'print' method here (every analysis pass should implement it).
//
// USAGE:
//    New PM
//      opt -load-pass-plugin=libHelloWorld.dylib -passes="hello-world" `\`
//        -disable-output <input-llvm-file>
//
//
// License: MIT
//=============================================================================
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include <fstream>
#include <sstream>

using namespace llvm;

//-----------------------------------------------------------------------------
// HelloWorld implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {

// Function to write header to the CSV file
void writeCSVHeader(std::ofstream &csvFile) {
    csvFile << "Function Name, Function Pointer Address, Instruction Address\n";
}

// Function to write a line to the CSV file
void writeCSVLine(std::ofstream &csvFile, const std::string &functionName, const std::string &address, const std::string &instruction) {
    csvFile << functionName << "," << address << "," << instruction << "\n";
}

// Function to print the address of the function pointer
void printFunctionPointerAddress(Value *CalledValue, LLVMContext &Context, std::ofstream &csvFile, const Function &F, const Instruction &I) {

    std::string addrStr;
    raw_string_ostream addrStream(addrStr);
    addrStream << CalledValue;
    
    std::string insStr;
    raw_string_ostream insStream(insStr);
    insStream << &I;

    // Write the function name, instruction, and pointer address to the CSV
    writeCSVLine(csvFile, F.getName().str(), addrStream.str(), insStream.str());
}

// FunctionPass that analyzes function pointers
struct HelloWorld : public PassInfoMixin<HelloWorld> {
    // Entry point for the pass
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        // Open CSV file
        std::ofstream csvFile;
        if (F.getName() == "main") {  // Open file for the first function (or adjust as needed)
            csvFile.open("function_pointers.csv", std::ios::out | std::ios::trunc);
            writeCSVHeader(csvFile);
        } else {
            csvFile.open("function_pointers.csv", std::ios::out | std::ios::app);
        }

        for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *Call = dyn_cast<CallBase>(&I)) {
                    if (auto *CalledFunction = Call->getCalledFunction()) {
                        // Direct function call, ignore.
                        continue;
                    } else {
                        Value *CalledValue = Call->getCalledOperand();
                        if (CalledValue->getType()->isPointerTy()) {
                            // Indirect function call
                            errs() << "Function pointer used in function: " << F.getName() << "\n";
                            errs() << "Instruction: " << I << "\n";
                            errs() << "Function pointed to: " << CalledValue << "\n";
                            printFunctionPointerAddress(CalledValue, F.getContext(), csvFile, F, I);
                        }
                    }
                }
            }
        }

        csvFile.close();  // Close the file after each function (or manage it as needed)
        return PreservedAnalyses::all();
    }

    static bool isRequired() { return true; }
};

}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getHelloWorldPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerPipelineEarlySimplificationEPCallback(
        [&](ModulePassManager &MPM, auto) {
          MPM.addPass(createModuleToFunctionPassAdaptor(HelloWorld()));
          return true;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "hello-world", "0.0.1", callback};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getHelloWorldPluginInfo();
}
