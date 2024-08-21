# To silence warning caused by Wundef.
add_definitions(-DGTEST_NO_LLVM_SUPPORT=0)

add_mlir_unittest(ToyUnitTests
  LexerTest.cpp
)
#target_link_libraries(ToyUnitTests
#  PRIVATE
#  LLVMSupport)
set(LLVM_LINK_COMPONENTS
  Core
  Support
)
