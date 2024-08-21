//===--- Parser.h -  --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the parser for the Toy language. It processes the Token
// provided by the Lexer and returns an AST.
//
//===----------------------------------------------------------------------===//
#ifndef TOY_PARSER_H
#define TOY_PARSER_H

#include "toy/AST.h"
#include "toy/Lexer.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace toy {

/// This is a simple recursive parser for the Toy language. It produces a well-
/// formed AST from a stream of Token supplied by the Lexer. No semantic checks
/// or symbol resolution is performed. For example, variables are referenced by
/// string and the code could reference and undeclared variable and the parsing
/// succeeds.
class Parser final {
public:
  /// Create a Parser for the supplied lexer.
  Parser(Lexer &lexer) : lexer(lexer) {}

  /// Parser a full Module. A module is a list of function definitions.
  std::unique_ptr<Module> parseModule() {
    lexer.getNextToken(); // prime the lexer

    // Parse functions one at a time and accumulate in this vector.
    std::vector<Function> functions;
    while (auto f = parseDefinition()) {
      functions.emplace_back(std::move(*f));
      if (lexer.getCurToken() == tok_eof)
        break;
    }
    // If we didn't reach EOF, there was an error during parsing
    if (lexer.getCurToken() != tok_eof)
      return parseError<Module>("nothing", "at end of module");

    return std::make_unique<Module>(std::move(functions));
  }

private:
  Lexer &lexer;

  /// Parse a return statement.
  /// return ::= return ; | return expr ;
  std::unique_ptr<ReturnExpr> parseReturn() {
    auto loc = lexer.getLastLocation();
    lexer.consume(tok_return);

    // return takes an optional argument
    std::optional<std::unique_ptr<Expr>> expr; // declare a undef var of optional some expr.
    if (lexer.getCurToken() != ';') { // not ; => there is an expression
      expr = parseExpression();       // parse it!
      if (!expr)
        return nullptr; // something is wrong: we have no ; and no expression there.
    }
    return std::make_unique<ReturnExpr>(std::move(loc), std::move(expr)); // just pass expr as an argument.
  }

  /// Parse a literal number.
  /// numberexpr ::= number
  std::unique_ptr<Expr> parseNumberExpr() {
    auto loc = lexer.getLastLocation();
    auto result =
        std::make_unique<NumberExpr>(std::move(loc), lexer.getValue());
    lexer.consume(tok_number);
    return std::move(result);
  }

  /// Parse a literal array expression.
  /// tensorLiteral ::= [ literalList ] | number
  /// literalList ::= tensorLiteral | tensorLiteral, literalList
  std::unique_ptr<Expr> parseTensorLiteralExpr() {
    auto loc = lexer.getLastLocation();
    lexer.consume(Token('['));

    // Hold the list of values at this nesting level.
    ExprList values;
    // Hold the dimensions for all the nesting inside this level.
    std::vector<int64_t> dims;
    // Let's go!
    do {
      // We can have either another nested array or a number literal.
      if (lexer.getCurToken() == '[') {
        values.emplace_back(parseTensorLiteralExpr()); // recursive parser!
        if (!values.back())
          return nullptr; // parse error in the nested array.
      } else {
        if (lexer.getCurToken() != tok_number)
          return parseError<Expr>("<num> or [", "in literal expression");
        values.emplace_back(parseNumberExpr());
      }

      // End of this list on ']'
      if (lexer.getCurToken() == ']')
        break;

      // Elements are separated by a comma.
      if (lexer.getCurToken() != ',')
        return parseError<Expr>("] or ,", "in literal expression");

      //lexer.getNextToken(); // eat ,
      lexer.consume(Token(',')); // eat ,
    } while (true);
    if (values.empty())
      return parseError<Expr>("<something>", "to fill literal expression");
    lexer.consume(Token(']')); // eat ]

    // psamolysov: Now we have all the values in the list, let's correct process it.
    /// Fill in the dimensions now. First the current nesting level:
    dims.emplace_back(values.size());

    /// If there is any nested array, process all of them and ensure that
    /// dimensions are uniform.
    if (llvm::any_of(values, [](std::unique_ptr<Expr> &expr) { // unique_ptr by reference :)
                                                               // this makes sense because values stores unique_ptrs
          return llvm::isa<LiteralExpr>(expr.get()); // we also parse LiteralExpr
        })) {
      auto *firstLiteral = llvm::dyn_cast<LiteralExpr>(values.front().get()); // we are sure there is a LiteralExpr but we
                                                                              // aren't sure this is the first, so that dyn_cast.
      if (!firstLiteral)
        return parseError<Expr>("uniform well-nested dimensions",
                                "inside literal expression");

      // Append the nested dimensions to the current level
      auto firstDims = firstLiteral->getDims();
      // because we repeat this for every literal, the firstLiteral has also accumulated all its nested dimensions.
      // nested! Not siblings! So here will be Literal <2, 3, 1>[ <3, 1>[<1>[0.2], <1>[0.3], <1>[0.4]], <3>[<1>...]].
      // <2, 3, 1> is dims, so, it contains copy of <3, 1> from the nested.
      dims.insert(dims.end(), firstDims.begin(), firstDims.end()); // aha! STL inserts BEFORE the first argument iterator because
                                                                   // it is very easy to pass end() there.
      
      // Sanity check that shape is uniform across all elements of the list.
      for (const auto &expr : values) {
        const auto *exprLiteral = llvm::cast<LiteralExpr>(expr.get()); // I believe in dyn_cast.
        if (!exprLiteral) // do we really need this check? If we aren't sure this is LiteralExpr we must use dyn_cast<>,
                          // if we aren't sure this has a value at all, we must use cast_if_present<>. dyn_cast_if_present<>
                          // when we sure in nothing.
          return parseError<Expr>("uniform well-nested dimensions",
                                  "inside literal expression");
        if (exprLiteral->getDims() != firstDims)
          return parseError<Expr>("uniform well-nested dimensions",
                                  "inside literal expression");
      }
    }

    return std::make_unique<LiteralExpr>(std::move(loc), std::move(values),
                                         std::move(dims)); // good pattern of immutability! Collect data somewhere,
                                         // then do all the required checks and then move data into the result (using a
                                         // constructor or a builder).
  }

  /// parenexpr ::= '(' expression ')'
  std::unique_ptr<Expr> parseParenExpr() {
    lexer.consume(Token('(')); // eat (.
    auto v = parseExpression(); // so, not a rocket science! we are sure here is terminal nonterminal terminal,
                                // just eat the first terminal, call the corresponding parseSMTH for the nonterminal,
                                // eat the second terminal and build an AST node. That's all.
    if (!v)
      return nullptr; // no expression - no our wrapper!

    if (lexer.getCurToken() != ')')
      return parseError<Expr>(")", "to close expression with parentheses");
    lexer.consume(Token(')')); // eat ). psamolysov: I believe Token(')') is easy to read then top_paranthese_close.
    return v;
  }

  /// identifier
  ///   ::= identifier
  ///   ::= identifier '(' expression ')'
  std::unique_ptr<Expr> parseIdentifierExpr() {
    std::string name(lexer.getId()); // getId() returns llvm::StringRef, we copy it to std::string to own
        // because when the lexer's state changes (we are calling lexer.getNextToken() soon), it "forgets" 
        // the owned string (this is how assignment in C++ works!) When someone returns a reference to you,
        // remember about the lifetimes.

    auto loc = lexer.getLastLocation();
    //lexer.getNextToken(); // eat identifier.
    lexer.consume(tok_identifier); // eat identifier.
    
    if (lexer.getCurToken() != '(') // simple variable ref.
      return std::make_unique<VariableExpr>(std::move(loc), name);

    // This is a function call. (psamolysov: so, one method may return nodes of different kinds and
    //   make some decisions. This is a meta idea of identifier (in AST), actually, this may be either a variable
    //   ref or a function call ('id(expression)')
    lexer.consume(Token('('));
    ExprList args; // do not forget to hold what is required for the node.
    if (lexer.getCurToken() != ')') {
      while (true) {
        if (auto arg = parseExpression())
          args.emplace_back(std::move(arg)); // const before declaration: const auto arg kills this moving!
        else
          return nullptr;

        if (lexer.getCurToken() == ')')
          break;
        
        if (lexer.getCurToken() != ',') // typical for a list of smth, if (while) not , - parse an expression if it is allowed.
          return parseError<Expr>(", or )", "in argument list");
        lexer.getNextToken();          
      }
    }
    lexer.consume(Token(')'));

    // It can be a builtin call to print.
    if (name == "print") {
      if (args.size() != 1)
        return parseError<Expr>("<single arg>", "as argument to print()"); // we always may return nullptr as a std::unique_ptr.

      return std::make_unique<PrintExpr>(std::move(loc), std::move(args[0]));
    }

    // Call to a user-defined function.
    return std::make_unique<CallExpr>(std::move(loc), std::move(name), std::move(args)); // if name is StringRef - compilation error,
                                                                                         // no ref to rvalue. Hm, no, no comp. error.
        // CallExpr takes the callee by ref, so we cannot move name
        // and the constructor has to make a copy of to be the owner of the string... oh.
        // let's rewrite.
  }

  /// primary
  ///   ::= identifierexpr
  ///   ::= numberexpr
  ///   ::= parenexpr
  ///   ::= tensorliteral
  std::unique_ptr<Expr> parsePrimary() { // this is a good idea to collect all common variants into a single parse method.
    switch(lexer.getCurToken()) {
    default:
      llvm::errs() << "unknown token '" << lexer.getCurToken()
                   << "' when expecting an expression\n";
      return nullptr;
    case tok_identifier:
      return parseIdentifierExpr();
    case tok_number:
      return parseNumberExpr();
    case '(':
      return parseParenExpr();
    case '[':
      return parseTensorLiteralExpr(); // we do not consume the discriminant ('(' or '[' or id) here,
                                       // this is a job for a particular parse method.
    case ';':
      return nullptr;
    case '}':
      return nullptr;
    }
  }

  /// Recursively parse the right hand side of a binary expression, the ExprPrec
  /// argument indicates the precedence of the current binary operator.
  ///
  /// binoprhs ::= ('+' primary)*
  std::unique_ptr<Expr> parseBinOpRHS(int exprPrec,
                                      std::unique_ptr<Expr> lhs) {
    // If this is a binop, find its precedence.
    while (true) {
      int tokPrec = getTokPrecedence();

      // If this is a binop that binds at least a tightly as the current binop,
      // consume it, otherwise we are done.
      if (tokPrec < exprPrec)
        return lhs;

      // Okay, we know this is a binop.
      int binOp = lexer.getCurToken();
      lexer.consume(Token(binOp));
      auto loc = lexer.getLastLocation();

      // Parse the primary expression after the binary operator.
      auto rhs = parsePrimary();
      if (!rhs)
        return parseError<Expr>("expression", "to complete binary operator");

      // If BinOp binds less tightly with rhs than the operator after rhs, let
      // the pending operator take rhs as its lhs.
      // psamolysov lhs + rhs * rhs2, rhs is lhs for *.
      int nextPrec = getTokPrecedence();
      if (tokPrec < nextPrec) {
        rhs = parseBinOpRHS(tokPrec + 1, std::move(rhs));
        if (!rhs)
          return nullptr;
      }

      // Merge lhs/RHS.
      lhs = std::make_unique<BinaryExpr>(std::move(loc), binOp,
                                         std::move(lhs), std::move(rhs));
    }
  }

  /// expression ::= primary binop rhs
  std::unique_ptr<Expr> parseExpression() {
    auto lhs = parsePrimary();
    if (!lhs)
      return nullptr;

    return parseBinOpRHS(0, std::move(lhs));
  }

  /// type ::= < shape_list >
  /// shape_list ::= num | num, shape_list
  std::unique_ptr<VarType> parseType() {
    if (lexer.getCurToken() != '<')
      return parseError<VarType>("<", "to begin type");
    lexer.consume(Token('<')); // eat <

    auto type = std::make_unique<VarType>(); // make a std::unique_ptr and reuse it later.
                          // this violates our common pattern of creating immutable nodes.

    while (lexer.getCurToken() == tok_number) {
      type->shape.push_back(lexer.getValue());
      lexer.getNextToken();
      if (lexer.getCurToken() == ',')
        lexer.getNextToken();
    }

    if (lexer.getCurToken() != '>')
      return parseError<VarType>(">", "to end type");
    lexer.consume(Token('>')); // eat >
    return type;
  }

  /// Parse a variable declaration, it starts with a `var` keyword followed by
  /// and identifier and an optional type (shape specification) before the
  /// initializer.
  /// decl ::= var identifier [ type ] = expr
  std::unique_ptr<VarDeclExpr> parseDeclaration() {
    if (lexer.getCurToken() != tok_var)
      return parseError<VarDeclExpr>("var", "to begin declaration");
    auto loc = lexer.getLastLocation();
    lexer.consume(tok_var); // eat var

    if (lexer.getCurToken() != tok_identifier) // when we have many possible variants, we can choose. When only one,
      return parseError<VarDeclExpr>("identifier", "after 'var' declaration"); // we must check and warn.

    std::string id(lexer.getId()); // see a comment somewhere above about ownership transferring between lexer and parser.
    lexer.consume(tok_identifier);

    std::unique_ptr<VarType> type; // Type is optional, it can be inferred.
    if (lexer.getCurToken() == '<') {
      type = parseType();
      if (!type)
        return nullptr;
    }

    if (!type)
      type = std::make_unique<VarType>();
    lexer.consume(Token('='));
    auto expr = parseExpression();
    return std::make_unique<VarDeclExpr>(std::move(loc), std::move(id),
                                         std::move(*type), std::move(expr));
      // non-consistent but, VarType is a wrapper on vector of ints, so we hold it in VarDeclExpr by value.
  }

  /// Parse a block: a list of expression separated by semicolons and wrapped in
  /// curly braces.
  ///
  /// block ::= { expression_list }
  /// expression_list ::= block_expr ; expression_list
  /// block_expr ::= decl | "return" | expr
  std::unique_ptr<ExprList> parseBlock() {
    if (lexer.getCurToken() != '{')
      return parseError<ExprList>("{", "to begin block");
    lexer.consume(Token('{'));

    auto exprList = std::make_unique<ExprList>();

    // Ignore empty expressions: swallow sequences of semicolons. // hm, I would look for a single one only, good idea to match
                                                                  // as many as it is possible.
    while (lexer.getCurToken() == ';')
      lexer.consume(Token(';'));

    while (lexer.getCurToken() != '}' && lexer.getCurToken() != tok_eof) {
      if (lexer.getCurToken() == tok_var) {
        // Variable declaration
        auto varDecl = parseDeclaration();
        if (!varDecl)
          return nullptr; // hm, looks as error, i'm not sure (return nullptr), not error - return an AST node.
        exprList->emplace_back(std::move(varDecl));           
      } else if (lexer.getCurToken() == tok_return) { // classic: check what we see and get the decision - call the required parse...
                                                      // a way for every | in the formula.
        // Return statement
        auto ret = parseReturn();
        if (!ret)
          return nullptr;
        exprList->emplace_back(std::move(ret));
      } else {
        // General expression
        auto expr = parseExpression();
        if (!expr)
          return nullptr; // we may not check if expr was optional
        exprList->emplace_back(std::move(expr));
      }
      // Ensure that elements are separated by semicolon.
      if (lexer.getCurToken() != ';')
        return parseError<ExprList>(";", "after expression");
      // when we are waiting for something but not sure - we check and use return parseError, but
      // when we are sure we use consume and if this is something else, this is OUR error, not the user,
      // we fail on the assert.

      // Ignore empty expressions: swallow sequences of semicolons
      while (lexer.getCurToken() == ';')
        lexer.consume(Token(';'));
    }
    
    if (lexer.getCurToken() != '}')
      return parseError<ExprList>("}", "to close block");

    lexer.consume(Token('}'));
    return exprList;

    // vector is very useful data type: on the stack there are only 3 elements: capacity, size and the pointer
    // and we may place it on the stack and collect the data: the data will be collected in the heap anyway.
    // then we can move into an immutable AST node.
  }

  /// prototype ::= def id '(' decl_list ')'
  /// decl_list ::= identifier | identifier, decl_list
  std::unique_ptr<Prototype> parsePrototype() {
    // we never pass something between parse functions in the parameters, only as a part of the object's state:
    // the state of the field 'lexer', for example. AST nodes we return and the caller collects them in its node
    // on the heap with (the smart pointer on the stack, more accurately, the node itself is on the heap).
    auto loc = lexer.getLastLocation();

    if (lexer.getCurToken() != tok_def) // good practice to guard: whether we are calling from the correct place on the "tape"
                                        // lexem stream.
      return parseError<Prototype>("def", "in prototype");
    lexer.consume(tok_def); // we are sure, we must be sure because we checked!

    if (lexer.getCurToken() != tok_identifier)
      return parseError<Prototype>("function name", "in prototype");

    std::string fnName(lexer.getId()); // good API but we mush be sure we have an ID here, no compile-time check is there.
    lexer.consume(tok_identifier);

    if (lexer.getCurToken() != '(')
      return parseError<Prototype>("(", "in prototype");
    lexer.consume(Token('('));

    std::vector<std::unique_ptr<VariableExpr>> args;
    if (lexer.getCurToken() != ')') {
      // I believe a check with parseError is requied, we are waiting for id but this may be not an id token.
      // So our rule dictates id here but we are parsing a user input and the user may write whatever he/she wants.
      if (lexer.getCurToken() != tok_identifier)
        return parseError<Prototype>("identifier", "after '(' in function parameter list");
      do {
        std::string name(lexer.getId());
        auto loc = lexer.getLastLocation();
        lexer.consume(tok_identifier);
        auto decl = std::make_unique<VariableExpr>(std::move(loc), std::move(name)); // TODO fix pass by ref there
        args.emplace_back(std::move(decl));
        if (lexer.getCurToken() != ',')
          break;
        lexer.consume(Token(','));
        if (lexer.getCurToken() != tok_identifier) // TODO replicate before the first iteration and change the text a bit.
          return parseError<Prototype>("identifier", "after ',' in function parameter list");
      } while (true);
    }
    if (lexer.getCurToken() != ')')
      return parseError<Prototype>(")", "to end function prototype");

    // success.
    lexer.consume(Token(')'));
    return std::make_unique<Prototype>(std::move(loc), std::move(fnName), std::move(args)); // TODO fix pass by ref there
  }

  /// Parse a function definition, we expect a prototype initiated with the
  /// `def` keyword, followed by a block containing a list of expressions.
  ///
  /// definition ::= prototype block
  std::unique_ptr<Function> parseDefinition() {
    auto proto = parsePrototype();
    if (!proto)
      return nullptr;

    if (auto block = parseBlock())
      return std::make_unique<Function>(std::move(proto), std::move(block));
    return nullptr;
  }

  /// Get the precedence of the pending binary operator token.
  int getTokPrecedence() {
    if (!isascii(lexer.getCurToken()))
      return -1;

    // 1 is lowest precedence.
    switch (static_cast<char>(lexer.getCurToken())) {
    case '-':
      return 20;
    case '+':
      return 20;
    case '*':
      return 40;
    default:
      return -1;
    }
  }

  /// Helper function to signal errors while parsing, it takes an argument
  /// indicating the expected token and another argument giving more context.
  /// Location is retrieved from the lexer to enrich the error message.
  template <typename R, typename T, typename U = const char *>
  std::unique_ptr<R> parseError(T &&expected, U &&context = "") {
    auto curToken = lexer.getCurToken();
    llvm::errs() << "Parse error (" << lexer.getLastLocation().line << ", "
                 << lexer.getLastLocation().col << "): expected '" << expected
                 << "' " << context << " but has Token " << curToken;
    if (isprint(curToken))
      llvm::errs() << " '" << (char)curToken << "'";
    llvm::errs() << "\n";
    return nullptr;
  }
};

} // namespace toy

#endif //TOY_PARSER_H
