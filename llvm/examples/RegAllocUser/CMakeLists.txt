set(LLVM_LINK_COMPONENTS
  Core
  Support
  MIRParser
  Passes
  nativecodegen
  )

add_llvm_example(RegAllocUser
  regalloc-user.cpp
  )
