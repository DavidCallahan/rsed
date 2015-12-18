//
//  AST.h
//  rsed
//
//  Created by David Callahan on 6/25/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#ifndef __rsed__AST__
#define __rsed__AST__
#include <string>
#include <regex>
#include "Symbol.h"
#include "StringRef.h"

class Statement;
class Expression;

class AST {
protected:
  int id;
  int sourceLine;
  static int next_id;

public:
  void dump() const;
  void processStrings();
  int getId() const { return id; }
  enum StopKind { StopAfter, StopAt };

  AST(int sourceLine = 0) : id(++next_id), sourceLine(sourceLine) {}

  static inline Statement *emptyStmt() { return nullptr; }
  static inline Expression *emptyExpr() { return nullptr; }

  static Statement *foreach (Expression *control, Statement * body,
                             int sourceLine);
  static Statement *copy(Expression *control, int sourceLine = 0);
  static Statement *skip(Expression *control, int sourceLine = 0);
  static Statement *skipOne(Expression *pattern, int sourceLine);
  static Statement *replace(Expression *control, Expression *pattern,
                            Expression *replacement, bool replaceAll,
                            int sourceLine);
  static Statement *replaceOne(Expression *pattern, Expression *replacement,
                               bool replaceAll, int sourceLine);
  static Statement *print(Expression *text, Expression *buffer);
  static Statement *error(Expression *text);
  static Statement *input(Expression *buffer);
  static Statement *output(Expression *buffer);

  static Expression *limit(int number, Expression *control);
  static Expression *limit(int number, int sourceLine);
  static Expression *pattern(std::string *pattern);
  static Expression *notExpr(Expression *pattern);
  static Statement *set(std::string *lhs, Expression *stringExpr);
  static Expression *integer(int value);
  static Expression *variable(std::string *var);
  static Expression *stringConst(std::string *constant);
  static Expression *match(Expression *pattern, Expression *target);
  static Expression *identifier(std::string *name);
  static Expression *call(std::string *name, Expression *args, unsigned callId,
                          int sourceLine);

  static Expression *fileBuffer(Expression *name);
  static Expression *memoryBuffer(std::string *name);
  static inline Expression *all() { return emptyExpr(); }

  // foreach control should not have NOT
  static std::string checkPattern(Expression *pattern);

  // top expressions should not have MATCH or NOT
  static std::string checkTopExpression(Expression *pattern);

  virtual bool isStatement() const = 0;

  enum StmtKind {
    ForeachN,
    SkipN,
    CopyN,
    ReplaceN,
    SplitN,
    IfStmtN,
    SetN,
    ColumnsN,
    PrintN,
    ErrorN,
    InputN,
    OutputN,
  };
  enum ExprKind {
    ControlN,
    VariableN,
    IntegerN,
    BufferN,
    StringConstN,
    CallN,
    ArgN,
    BinaryN,
  };
  enum WalkResult { StopW, ContinueW, SkipChildrenW, SkipExpressionsW };
};

class Statement : public AST {
  Statement *next = nullptr;

protected:
  Statement(int sourceLine = 0) : AST(sourceLine) {}

public:
  bool isStatement() const override { return true; }
  virtual StmtKind kind() const = 0;
  Statement *getNext() const { return next; }
  void setNext(Statement *next) { this->next = next; }
  void dumpOne();

  template <typename ACTION> WalkResult walk(const ACTION &a);
  template <typename ACTION> WalkResult walkExprs(const ACTION &a);
  template <typename T> static T *list(T *car, T *cdr) {
    if (!car)
      return cdr;
    T *tail = car;
    while (tail->next)
      tail = (T *)tail->next;
    tail->next = cdr;
    return car;
  }
};

typedef class Binary *BinaryP;
class Expression : public AST {
protected:
  Expression(int sourceLine = 0) : AST(sourceLine) {}

public:
  enum Operators {
    ADD,
    SUB,
    MUL,
    DIV,
    NEG,
    MATCH,
    REPLACE,
    CONCAT,
    LT,
    LE,
    EQ,
    NE,
    GE,
    GT,
    NOT,
    AND,
    OR,
    LOOKUP,
  };

  bool isStatement() const override { return false; }
  virtual ExprKind kind() const = 0;
  //  Expression *getNext() const { return (Expression *)next; }
  //  void setNext(Expression *next) { this->next = next; }

  template <typename ACTION> WalkResult walkDown(const ACTION &a);
  template <typename ACTION> WalkResult walkUp(const ACTION &a);
  BinaryP isOp(Operators op);
  static const char *opName(Operators op);
};

class Binary : public Expression {
public:
  Operators op;
  Expression *left;
  Expression *right;

  Binary(Operators op, Expression *left, Expression *right, int sourceLine)
      : Expression(sourceLine), op(op), left(left), right(right) {}
  ExprKind kind() const override { return BinaryN; }
  bool isOp(Operators op) { return op == this->op; }
};
typedef Binary *BinaryP;
inline BinaryP Expression::isOp(Operators op) {
  return (kind() == BinaryN && BinaryP(this)->isOp(op) ? BinaryP(this)
                                                       : nullptr);
}

class Foreach : public Statement {
public:
  Expression *control;
  Statement *body;

  Foreach(Expression *control, Statement *body, int sourceLine)
      : Statement(sourceLine), control(control), body(body) {}
  static StmtKind typeKind() { return ForeachN; }
  StmtKind kind() const override { return typeKind(); }
};
typedef Foreach *ForeachP;

class Control : public Expression {
  StopKind stopKind;
  int limit;

public:
  Expression *pattern;

  enum { NO_LIMIT = -1 };
  Control(StopKind stopKind, Expression *pattern, int sourceLine)
      : stopKind(stopKind), limit(NO_LIMIT), pattern(pattern) {}
  void setLimit(int num) { limit = num; }
  bool hasLimit() const { return limit > 0; }
  ExprKind kind() const override { return ControlN; }

  int getLimit() const { return limit; }
  StopKind getStopKind() const { return stopKind; }
};
inline Expression *AST::limit(int number, Expression *control) {
  ((Control *)control)->setLimit(number);
  return control;
}
inline Expression *AST::limit(int number, int sourceLine) {
  auto c = new Control(StopAfter, nullptr, sourceLine);
  c->setLimit(number);
  return c;
}

class CopySkip : public Statement {
protected:
  CopySkip(int sourceLine) : Statement(sourceLine) {}
};
class Copy : public CopySkip {
public:
  Copy(int souceLine) : CopySkip(sourceLine) {}
  static StmtKind typeKind() { return CopyN; }
  StmtKind kind() const override { return typeKind(); }
};
inline Statement *AST::copy(Expression *control, int sourceLine) {
  return new Foreach(control, new Copy(sourceLine), sourceLine);
}

class Skip : public CopySkip {
public:
  Skip(int sourceLine) : CopySkip(sourceLine) {}
  static StmtKind typeKind() { return SkipN; }
  StmtKind kind() const override { return typeKind(); }
};
inline Statement *AST::skip(Expression *control, int sourceLine) {
  return new Foreach(control, new Skip(sourceLine), sourceLine);
}

class Replace : public Statement {
public:
  Expression *pattern;
  Expression *replacement;
  bool optAll;
  Replace(Expression *pattern, Expression *replacement, bool optAll,
          int sourceLine)
      : Statement(sourceLine), pattern(pattern), replacement(replacement),
        optAll(optAll) {}
  static StmtKind typeKind() { return ReplaceN; }
  StmtKind kind() const override { return typeKind(); }
};
inline Statement *AST::replace(Expression *control, Expression *pattern,
                               Expression *replacement, bool optAll,
                               int sourceLine) {
  Statement *body = new Replace(pattern, replacement, optAll, sourceLine);
#ifndef IMPLICIT_FOREACH_COPY
  body = Statement::list<Statement>(body, new Copy(sourceLine));
#endif
  return new Foreach(control, body, sourceLine);
}
inline Statement *AST::replaceOne(Expression *pattern, Expression *replacement,
                                  bool optAll, int sourceLine) {
  return new Replace(pattern, replacement, optAll, sourceLine);
}
class Split : public Statement {
public:
  Expression *separator;
  Expression *target;
  Split(Expression *separator, Expression *target, int sourceLine)
      : Statement(sourceLine), separator(separator), target(target) {}
  static StmtKind typeKind() { return SplitN; }
  StmtKind kind() const override { return typeKind(); }
};

class IfStatement : public Statement {
public:
  Expression *predicate; // TODO rename predicate
  Statement *thenStmts;
  Statement *elseStmts;

  IfStatement(Expression *predicate, Statement *thenStmts, Statement *elseStmts,
              int sourceLine)
      : Statement(sourceLine), predicate(predicate), thenStmts(thenStmts),
        elseStmts(elseStmts) {}
  static StmtKind typeKind() { return IfStmtN; }
  StmtKind kind() const override { return typeKind(); }
};
inline Statement *AST::skipOne(Expression *pattern, int sourceLine) {
  return new IfStatement(pattern, new Skip(sourceLine), nullptr, sourceLine);
}

class Columns : public Statement {
public:
  Expression *columns;
  Expression *inExpr;
  Columns(Expression *columns, Expression *inExpr, int sourceLine)
      : columns(columns), inExpr(inExpr) {}
  static StmtKind typeKind() { return ColumnsN; }
  StmtKind kind() const override { return typeKind(); }
};

class Variable : public Expression {
  Symbol &symbol;

public:
  Variable(Symbol &symbol) : symbol(symbol) {}
  ExprKind kind() const override { return VariableN; }
  const std::string &getName() const { return symbol.getName(); }
  Symbol &getSymbol() const { return symbol; }
};
inline Expression *AST::variable(std::string *var) {
  auto v = new Variable(*Symbol::findSymbol(*var));
  delete var;
  return v;
}

class StringConst : public Expression {
  StringRef constant;

public:
  StringConst(std::string *constant) : constant(constant) {}
  ExprKind kind() const override { return StringConstN; }
  const StringRef &getConstant() const { return constant; }
  void setConstant(StringRef constant) { this->constant = constant; }
};
inline Expression *AST::stringConst(std::string *constant) {
  return new StringConst(constant);
}

class Integer : public Expression {
  int value;

public:
  Integer(int value) : value(value) {}
  ExprKind kind() const override { return IntegerN; }
  int getValue() const { return value; }
};
inline Expression *AST::integer(int value) { return new Integer(value); }

class Set : public Statement {
public:
  Expression *rhs;
  Expression *lhs;

  Set(Expression *lhs, Expression *rhs, int sourceLine)
      : Statement(sourceLine), rhs(rhs), lhs(lhs) {}

  static StmtKind typeKind() { return SetN; }
  StmtKind kind() const override { return typeKind(); }
};

class Print : public Statement {
public:
  Expression *text;
  Expression *buffer;

  Print(Expression *text, Expression *buffer) : text(text), buffer(buffer) {}
  static StmtKind typeKind() { return PrintN; }
  StmtKind kind() const override { return typeKind(); }
};
inline Statement *AST::print(Expression *text, Expression *buffer) {
  return new Print(text, buffer);
}

class Error : public Statement {

public:
  Expression *text;

  Error(Expression *text) : text(text) {}
  static StmtKind typeKind() { return ErrorN; }
  StmtKind kind() const override { return typeKind(); }
};
inline Statement *AST::error(Expression *text) { return new Error(text); }

class Buffer : public Expression {
  Expression *fileName;
  std::string *bufferName;

public:
  Buffer(Expression *fileName) : fileName(fileName), bufferName(nullptr) {}
  Buffer(std::string *bufferName) : fileName(nullptr), bufferName(bufferName) {}
  ExprKind kind() const override { return BufferN; }
  Expression *getFileName() const { return fileName; }
  void setFileName(Expression *fileName) { this->fileName = fileName; }
  const std::string &getBufferName() const { return *bufferName; }
  bool isFile() const { return getFileName(); }
};
inline Expression *AST::fileBuffer(Expression *name) {
  return new Buffer(name);
}
inline Expression *AST::memoryBuffer(std::string *name) {
  return new Buffer(name);
}

class Input : public Statement {
public:
  Expression *buffer;

  Input(Expression *buffer) : buffer(buffer) {}
  static StmtKind typeKind() { return InputN; }
  StmtKind kind() const override { return typeKind(); }
};
inline Statement *AST::input(Expression *buffer) { return new Input(buffer); }

class Output : public Statement {

public:
  Expression *buffer;
  Output(Expression *buffer) : buffer(buffer) {}
  static StmtKind typeKind() { return OutputN; }
  StmtKind kind() const override { return typeKind(); }
};
inline Statement *AST::output(Expression *buffer) { return new Output(buffer); }

class Arg : public Expression {
public:
  Expression *value;
  Arg *nextArg;
  Arg(Expression *value, Arg *nextArg, int souceLine)
      : Expression(sourceLine), value(value), nextArg(nextArg) {}
  ExprKind kind() const override { return ArgN; }
};

class Call : public Expression {
  std::string name;
  unsigned callId;

public:
  Arg *args;
  Call(std::string name, Arg *args, unsigned callId, int sourceLine)
      : Expression(sourceLine), name(name), callId(callId), args(args) {}
  ExprKind kind() const override { return CallN; }

  unsigned getCallId() const { return callId; }
  const std::string &getName() const { return name; }
  void setCallId(unsigned callId) { this->callId = callId; }
};
typedef Call *CallP;

template <typename T> T *isa(Statement *stmt) {
  return (stmt->kind() == T::typeKind() ? (T *)stmt : nullptr);
}

// TODO arithmetic ---  comparisons, booleans, (+ - * /)
// TODO -- optimization pass to pull string cmoparison and regular expression
//    compilation out of loops.
// TODO -- add better error handlings by tagging operations with line numbers
//    and reporting source script line (and maybe variable values mentioned in
//    the line?)
// TODO work in input/output. Add "tempfile() to support
//        $temp = tempfile()
//        output $temp
//        ...
//        input $temp
//       don't worry about in-memory buffering
// TODO add "columns .... in expr"

#endif /* defined(__rsed__AST__) */
