# For a better template to copy, see examples/standalone
include_directories(include)
add_subdirectory(include)

if(MLIR_INCLUDE_TESTS)
  if(EXISTS ${LLVM_THIRD_PARTY_DIR}/unittest/googletest/include/gtest/gtest.h)
    add_subdirectory(unittests)
  else()
    message(WARNING "gtest not found, unittests will not be available")
  endif()
endif()

set(LLVM_LINK_COMPONENTS
  Core
  Support
  )

add_toy_chapter(toyc-chmy
  toyc.cpp
  parser/AST.cpp

  DEPENDS
  )
