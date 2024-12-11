//===- LexerTest.cpp - Toy lexer unit tests -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "toy/Lexer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace toy;

using ::llvm::StringRef;
using ::testing::Eq;

TEST(LexerTest, LastLocation) {
  StringRef emptyText = "";
  LexerBuffer lexer{emptyText.begin(), emptyText.end(), "<nofile>"};
  // getLastLocation is not initialized here.
  lexer.getNextToken(); // prime the lexer
  EXPECT_THAT(*lexer.getLastLocation().file, Eq("<nofile>"));
  EXPECT_THAT(lexer.getLastLocation().line, Eq(1));
}

TEST(LexerTest, ZeroLineColumnNumbers) {
  StringRef emptyText = "";
  LexerBuffer lexer{emptyText.begin(), emptyText.end(), "<nofile>"};
  // getLastLocation is not initialized here.
  lexer.getNextToken(); // prime the lexer
  EXPECT_THAT(lexer.getCurToken(), Eq(tok_eof));
  EXPECT_THAT(*lexer.getLastLocation().file, Eq("<nofile>"));
  // Lexer *always* starts with "\n" and implicitly adds "\n" to the end of
  // every passed text. This is why getLastLocation().line and getLine() both
  // are equals to 1 at the end of lexing the empty string.
  EXPECT_THAT(lexer.getLastLocation().line, Eq(1));
  EXPECT_THAT(lexer.getLastLocation().col, Eq(0));
  EXPECT_THAT(lexer.getLine(), Eq(1));
  EXPECT_THAT(lexer.getCol(), Eq(0));
}

TEST(LexerTest, EmptyLine) {
  StringRef emptyText = "";
  LexerBuffer lexer{emptyText.begin(), emptyText.end(), "<nofile>"};
  lexer.getNextToken(); // prime the lexer
  EXPECT_THAT(lexer.getCurToken(), Eq(tok_eof));
}

TEST(LexerTest, NoPrimeTheLexer) {
  StringRef code = "la-la-la";
  LexerBuffer lexer{code.begin(), code.end(), "<nofile>"};
  EXPECT_THAT(lexer.getCurToken(), Eq(tok_eof));
}

TEST(LexerTest, Identifier) {
  StringRef code = "la-la-la";
  LexerBuffer lexer{code.begin(), code.end(), "<nofile>"};
  lexer.getNextToken(); // prime the lexer
  EXPECT_THAT(lexer.getCurToken(), Eq(tok_identifier));
  EXPECT_THAT(lexer.getId(), Eq("la"));
  // the location of the current token - Id("la") (location starts from 1:1)
  EXPECT_THAT(*lexer.getLastLocation().file, Eq("<nofile>"));
  EXPECT_THAT(lexer.getLastLocation().line, Eq(1));
  EXPECT_THAT(lexer.getLastLocation().col, Eq(1));
  // the location of the next to be read token.
  EXPECT_THAT(lexer.getLine(), Eq(1));
  EXPECT_THAT(lexer.getCol(), Eq(3));
}

TEST(LexerTest, BinaryOperation) {
  StringRef code = "a - b";
  LexerBuffer lexer{code.begin(), code.end(), "<nofile>"};
  lexer.getNextToken(); // prime the lexer
  EXPECT_THAT(lexer.getCurToken(), Eq(tok_identifier));
  // getCurToken() is an idempotent operation
  EXPECT_THAT(lexer.getCurToken(), Eq(tok_identifier));
  EXPECT_THAT(lexer.getId(), Eq("a"));

  EXPECT_THAT(lexer.getLastLocation().line, Eq(1));
  EXPECT_THAT(lexer.getLastLocation().col, Eq(1));
  auto nextLine = lexer.getLine();
  auto nextCol = lexer.getCol();
  lexer.getNextToken();
  int binOp = lexer.getCurToken();
  EXPECT_THAT(binOp, '-');
  EXPECT_THAT(lexer.getLastLocation().line, nextLine);
  EXPECT_THAT(lexer.getLastLocation().col, nextCol + 1 /*for a space*/);
  nextLine = lexer.getLine();
  nextCol = lexer.getCol();

  // Okay, we know this is a binop.
  lexer.consume(Token(binOp));
  EXPECT_THAT(lexer.getCurToken(), Eq(tok_identifier));
  EXPECT_THAT(lexer.getId(), "b");
  EXPECT_THAT(lexer.getLastLocation().line, nextLine);
  EXPECT_THAT(lexer.getLastLocation().col, nextCol + 1 /*for a space*/);
  nextLine = lexer.getLine();
  nextCol = lexer.getCol();

  lexer.consume(tok_identifier);
  EXPECT_THAT(lexer.getCurToken(), Eq(tok_eof));
  EXPECT_THAT(lexer.getLastLocation().line, nextLine);
  EXPECT_THAT(lexer.getLastLocation().col, nextCol);
}