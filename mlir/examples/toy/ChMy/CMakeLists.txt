# For a better template to copy, see examples/standalone
include_directories(include)
add_subdirectory(include)
add_subdirectory(unittest)

set(LLVM_LINK_COMPONENTS
  Core
  Support
)

add_toy_chapter(toyc-chmy
  toyc.cpp
  parser/AST.cpp

  DEPENDS
)
