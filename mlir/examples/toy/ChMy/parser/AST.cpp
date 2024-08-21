#include "toy/AST.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

using namespace toy;

namespace {

// RAII helper to manage increasing/decreasing the identation we traverse the
// AST.
struct Indent {
  Indent(int &level) : level(level) { ++level; }
  ~Indent() { --level; }
  int &level;
};

/// Helper class that implements the AST tree traversal and print the nodes
// along the way. The only data member is the current identation level.
class ASTDumper {
public:
  void dump(const Module *node);

private:
  void dump(const VarType &type);
  void dump(const VarDeclExpr *varDecl);
  void dump(const Expr *expr);
  void dump(const ExprList *exprList);
  void dump(const NumberExpr *num);
  void dump(const LiteralExpr *literal);
  void dump(const VariableExpr *var);
  void dump(const ReturnExpr *ret);
  void dump(const BinaryExpr *binOp);
  void dump(const CallExpr *call);
  void dump(const PrintExpr *print);
  void dump(const Prototype *proto);
  void dump(const Function *func);

  // Actually print spaces matching the current identation level
  void indent() {
    for (int i = 0; i < curIndent; i++)
      llvm::errs() << " ";
  }
  int curIndent = 0;
};

} // anonymous namespace

/// Return a formatted string for the location of any node
template <typename T>
static std::string loc(T *node) {
  const auto &loc = node->loc();
  return (llvm::Twine("@") + *loc.file + ":" + llvm::Twine(loc.line) + ":" +
          llvm::Twine(loc.col))
      .str();
}

// Helper Macro to bump the indentation level and print the leading spaces for
// the current indentations
#define INDENT()                                                               \
  Indent level_(curIndent);                                                    \
  indent();

/// Dispatch to a generic expression to the appropriate subclass using RTTI
void ASTDumper::dump(const Expr *expr) {
  llvm::TypeSwitch<const Expr *>(expr)
      .Case<BinaryExpr, CallExpr, LiteralExpr, NumberExpr, PrintExpr,
            ReturnExpr, VarDeclExpr, VariableExpr>(
          [&](auto *node) { this->dump(node); }) // starting from C++23, there is a more beautiful solution for recursive visitors.
      .Default([&](const Expr *) {
        // No match, fallback to a generic message
        INDENT();
        llvm::errs() << "<unknown Expr, kind " << expr->getKind() << ">";
      });
}

/// A variable declaration is printing the variable name, the type, and then
/// recurse in the initializer value.
void ASTDumper::dump(const VarDeclExpr *varDecl) {
  INDENT();
  llvm::errs() << "VarDecl " << varDecl->getName();
  dump(varDecl->getType());
  llvm::errs() << " " << loc(varDecl) << "\n";
  dump(varDecl->getInitVal());
}

/// A "block", or a list of expressions.
void ASTDumper::dump(const ExprList *exprList) {
  INDENT();
  llvm::errs() << "Block {\n";
  for (const auto &expr : *exprList) {
    dump(expr.get());
  }
  indent();
  llvm::errs() << "} // Block\n";
}

/// A literal number, just print the value.
void ASTDumper::dump(const NumberExpr *num) {
  INDENT();
  llvm::errs() << num->getValue() << " " << loc(num) << "\n";
}

/// Helper to print recursively a literal. This handles nested array like:
///     [ [ 1, 2 ], [ 3, 4 ] ]
/// We print out such array with the dimensions spelled out at every level:
///     <2, 2>[<2>[ 1, 2 ], <2>[ 3, 4 ] ]
static void printLitHelper(const Expr *litOrNum) {
  // Inside a literal expression we can have either a number or another literal
  if (const auto *num = llvm::dyn_cast<NumberExpr>(litOrNum)) {
    llvm::errs() << num->getValue();
    return;
  }
  const auto *literal = llvm::cast<LiteralExpr>(litOrNum);

  // Print the dimension for this literal first
  llvm::errs() << "<";
  llvm::interleaveComma(literal->getDims(), llvm::errs());
  llvm::errs() << ">";

  // Now print the content, recursing on every element of the list
  llvm::errs() << "[ ";
  llvm::interleaveComma(literal->getValues(), llvm::errs(),
                        [&](const auto &elt) { printLitHelper(elt.get()); });
  llvm::errs() << "]"; // or " ]"?
}

/// Print a literal, see the recursive helper above for the implementation.
void ASTDumper::dump(const LiteralExpr *literal) {
  INDENT();
  llvm::errs() << "Literal: ";
  printLitHelper(literal);
  llvm::errs() << " " << loc(literal) << "\n";
}

/// Print a variable reference (just a name).
void ASTDumper::dump(const VariableExpr *var) {
  INDENT();
  llvm::errs() << "var: " << var->getName() << " " << loc(var) << "\n";
}

/// Return statement print the return and its (optional) argument.
void ASTDumper::dump(const ReturnExpr *ret) {
  INDENT();
  llvm::errs() << "Return\n";
  if (ret->getExpr().has_value())
    return dump(*ret->getExpr());
  {
    INDENT();
    llvm::errs() << "(void)\n";
  }
}

/// Print a binary operation, first the operator, then recurse into LHS and RHS.
void ASTDumper::dump(const BinaryExpr *binOp) {
  INDENT();
  llvm::errs() << "BinOp: " << binOp->getOp() << " " << loc(binOp) << "\n";
  dump(binOp->getLHS());
  dump(binOp->getRHS());
}

/// Print a call expression, first the callee name and the list of args by
/// recursing into each individual argument.
void ASTDumper::dump(const CallExpr *call) {
  INDENT();
  llvm::errs() << "Call '" << call->getCallee() << "' [" << loc(call) << "\n";
  for (const auto &arg : call->getArgs())
    dump(arg.get());
  indent();
  llvm::errs() << "]\n";
}

/// Print a builtin print call, first the builtin name and then the argument.
void ASTDumper::dump(const PrintExpr *print) {
  INDENT();
  llvm::errs() << "Print [" << loc(print) << "\n";
  dump(print->getArg());
  indent();
  llvm::errs() << "]\n";
}

/// Print type: only the shape is printed in between '<' and '>'.
void ASTDumper::dump(const VarType &type) { // everything by ptr and only VarType by ref.
  llvm::errs() << "<";
  llvm::interleaveComma(type.shape, llvm::errs());
  llvm::errs() << ">";
}

/// Print a function prototype, first the function name, and then the list of
/// parameter names.
void ASTDumper::dump(const Prototype *proto) {
  INDENT();
  llvm::errs() << "Proto '" << proto->getName() << "' " << loc(proto) << "\n";
  indent();
  llvm::errs() << "Params: [";
  llvm::interleaveComma(proto->getArgs(), llvm::errs(), [](const auto &arg) {
    llvm::errs() << arg->getName();
  });
  llvm::errs() << "]\n";
}

/// Print a function, first the prototype and then the body.
void ASTDumper::dump(const Function *func) {
  INDENT();
  llvm::errs() << "Function \n";
  dump(func->getPrototype());
  dump(func->getBody());
}

/// Print a module, actually loop over the functions and print them in sequence.
void ASTDumper::dump(const Module *module) {
  INDENT();
  llvm::errs() << "Module:\n";
  for (const auto &f : *module)
    dump(&f);
}

namespace toy {

// Public API
void dump(const Module &module) { ASTDumper().dump(&module); }

} // namespace toy
