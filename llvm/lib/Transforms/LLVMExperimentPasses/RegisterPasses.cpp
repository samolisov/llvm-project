//========================================================================
// FILE:
//    RegisterPasses.cpp
//
// DESCRIPTION:
// Registers a group of passes from the LLVMExperimentPasses library inside
// the pass manager. The file is required for in-box LLVM building.
//
// License: MIT
//========================================================================

#include "llvm/InitializePasses.h"
#include "llvm/PassRegistry.h"

using namespace llvm;

/// initializeLLVMExperimentPasses - Initialize all passes in the
/// LLVMExperimentPasses library.
void llvm::initializeLLVMExperimentPasses(PassRegistry &Registry) {
  initializeLegacyFunctionArgumentUsagePassPass(Registry);
}
