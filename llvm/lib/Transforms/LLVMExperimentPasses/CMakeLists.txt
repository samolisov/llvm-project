add_llvm_library(LLVMExperimentPasses
  FunctionArgumentUsagePass.cpp
  FunctionArgumentUsagePrinterPass.cpp
  RegisterPasses.cpp

  DEPENDS
  intrinsics_gen
  )
