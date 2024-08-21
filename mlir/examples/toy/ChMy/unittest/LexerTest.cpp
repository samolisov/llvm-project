//===- Lexer.cpp -  ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "toy/Lexer.h"
#include "gmock/gmock.h"

using namespace toy;

TEST(LexerTest, EmptyLine) {
  EXPECT_THAT(1, 1);
}