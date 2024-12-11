//===- AST.h - Node definition for the Toy AST ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the AST for the Toy language. It is optimized for
// simplicity, not efficiency. The AST forms a tree structure where each node
// references its children using std::unique_ptr<>.
//
//===----------------------------------------------------------------------===//
#ifndef TOY_AST_H
#define TOY_AST_H

#include "toy/Lexer.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include <optional>
#include <utility>
#include <vector>

namespace toy {

/// A variable type with shape information.
struct VarType {
  std::vector<int64_t> shape;
};

/// Base class for all expression nodes.
class Expr {
public:
  enum ExprKind {
    Expr_VarDecl,
    Expr_Return,
    Expr_Num,
    Expr_Literal,
    Expr_Var,
    Expr_BinOp,
    Expr_Call,
    Expr_Print,
  };

  Expr(ExprKind kind, Location location)
      : kind(kind), location(std::move(location)) {}

  virtual ~Expr() = default;

  ExprKind getKind() const { return kind; }

  const Location &loc() const { return location; }

private:
  const ExprKind kind;
  Location location;
};

/// A block-list of expressions.
using ExprList = std::vector<std::unique_ptr<Expr>>;

/// Expression class for numeric literals like "1.0".
class NumberExpr : public Expr {
public:
  NumberExpr(Location loc, double val)
      : Expr(Expr_Num, std::move(loc)), val(val) {}

  double getValue() const { return val; }

  /// LLVM style RTTI
  static bool classof(const Expr *c) { return c->getKind() == Expr_Num; }

private:
  double val;
}; 

/// Expression class for a literal value
class LiteralExpr : public Expr {
public:
  LiteralExpr(Location loc, ExprList values, std::vector<int64_t> dims)
      : Expr(Expr_Literal, std::move(loc)), values(std::move(values)),
        dims(std::move(dims)) {}

  llvm::ArrayRef<std::unique_ptr<Expr>> getValues() const { return values; }

  llvm::ArrayRef<int64_t> getDims() const { return dims; }

  /// LLVM style RTTI
  static bool classof(const Expr *c) { return c->getKind() == Expr_Literal; }

private:
  ExprList values;
  std::vector<int64_t> dims;
};

/// Expression class for referencing a variable, like "a".
class VariableExpr : public Expr {
public:
  VariableExpr(Location loc, llvm::StringRef name)
    : Expr(Expr_Var, std::move(loc)), name(name) {}

  llvm::StringRef getName() const { return name; }

  /// LLVM style RTTI
  static bool classof(const Expr *c) { return c->getKind() == Expr_Var; }

private:
  std::string name; // an AST node must be an owner even though the external API
                    // is based on references.
};

/// Expression class for defining a variable.
class VarDeclExpr : public Expr {
public:
  VarDeclExpr(Location loc, llvm::StringRef name, VarType type,
              std::unique_ptr<Expr> initVal)
      : Expr(Expr_VarDecl, std::move(loc)), name(name), type(std::move(type)),
        initVal(std::move(initVal)) {}

  llvm::StringRef getName() const { return name; }

  const VarType &getType() const { return type; }

  const Expr *getInitVal() const { return initVal.get(); }

  /// LLVM style RTTI
  static bool classof(const Expr *c) { return c->getKind() == Expr_VarDecl; }

private:
  std::string name;
  VarType type;
  std::unique_ptr<Expr> initVal; // someone must be an owner...
};

/// Expression class for a return operator.
class ReturnExpr : public Expr {
public:
  ReturnExpr(Location loc, std::optional<std::unique_ptr<Expr>> expr)
    : Expr(Expr_Return, std::move(loc)), expr(std::move(expr)) {}

  std::optional<const Expr *> getExpr() const {
    if (expr.has_value())
      return expr->get();
    return std::nullopt;
  }

  /// LLVM style RTTI
  static bool classof(const Expr *c) { return c->getKind() == Expr_Return; }

private:
  std::optional<std::unique_ptr<Expr>> expr;
};

class BinaryExpr : public Expr {
public:
  BinaryExpr(Location loc, char op, std::unique_ptr<Expr> lhs,
             std::unique_ptr<Expr> rhs)
      : Expr(Expr_BinOp, std::move(loc)), op(op), lhs(std::move(lhs)),
        rhs(std::move(rhs)) {}

  char getOp() const { return op; }

  const Expr *getLHS() const { return lhs.get(); }

  const Expr *getRHS() const { return rhs.get(); }

  /// LLVM style RTTI
  static bool classof(const Expr *c) { return c->getKind() == Expr_BinOp; }

private:
  char op;
  std::unique_ptr<Expr> lhs, rhs;
};

/// Expression class for function calls.
class CallExpr : public Expr {
public:
  CallExpr(Location loc, llvm::StringRef callee, ExprList args)
      : Expr(Expr_Call, std::move(loc)), callee(callee), args(std::move(args)) {
  }

  llvm::StringRef getCallee() const { return callee; }

  llvm::ArrayRef<std::unique_ptr<Expr>> getArgs() const { return args; }

  /// LLVM style RTTI
  static bool classof(const Expr *c) { return c->getKind() == Expr_Call; }

private:
  std::string callee;
  ExprList args;
};

/// Expression class for builtin print calls.
class PrintExpr : public Expr {
public:
  PrintExpr(Location loc, std::unique_ptr<Expr> arg)
    : Expr(Expr_Print, std::move(loc)), arg(std::move(arg)) {}

  const Expr *getArg() const { return arg.get(); }

  /// LLVM style RTTI
  static bool classof(const Expr *c) { return c->getKind() == Expr_Print; }

private:
  std::unique_ptr<Expr> arg;
};

/// This class represents the "prototype" for a function, which captures its
/// name, and its argument names (thus implicitly the number of arguments the
/// function takes).
class Prototype {
public:
  Prototype(Location loc, llvm::StringRef name,
            std::vector<std::unique_ptr<VariableExpr>> args)
      : location(std::move(loc)), name(name), args(std::move(args)) {}

  const Location &loc() const { return location; }

  llvm::StringRef getName() const { return name; }

  llvm::ArrayRef<std::unique_ptr<VariableExpr>> getArgs() const { return args; }

private:
  Location location;
  std::string name;
  std::vector<std::unique_ptr<VariableExpr>> args;
};

/// This class represents a function definition itself.
class Function {
public:
  Function(std::unique_ptr<Prototype> proto, std::unique_ptr<ExprList> body)
    : proto(std::move(proto)), body(std::move(body)) {}

  const Prototype *getPrototype() const { return proto.get(); }

  const ExprList *getBody() const { return body.get(); }

private:
  std::unique_ptr<Prototype> proto;
  std::unique_ptr<ExprList> body; // wow! Not only store a vector but
                                  // a unique_ptr to a vector.
};

/// This class represents a list of functions to be processed together.
class Module {
public:
  Module(std::vector<Function> functions) : functions(std::move(functions)) {}

  // good idea, but not! llvm::ArrayRef<Function> getFunctions() const { return functions; }
  auto begin() { return functions.begin(); }
  auto begin() const { return functions.begin(); }
  auto end() { return functions.end(); }
  auto end() const { return functions.end(); } // LLVM style!

private:
  std::vector<Function> functions; // hm, here we have a vector of Functions,
                                   // as values, not unqiue_ptr or ptrs at all.
};

void dump(const Module &);

} // namespace toy

#endif //TOY_AST_H
