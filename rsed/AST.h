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
#include "Value.h"

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
  int getSourceLine() const { return sourceLine; }

  AST(int sourceLine) : id(++next_id), sourceLine(sourceLine) {}

  static Statement *foreach (Expression *control, Statement * body,
                             int sourceLine);
  static Statement *copy(Expression *control, int sourceLine);
  static Statement *skip(Expression *control, int sourceLine);
  static Statement *skipOne(Expression *pattern, int sourceLine);
  static Statement *replace(Expression *control, Expression *pattern,
                            Expression *replacement, bool replaceAll,
                            int sourceLine);
  static Statement *replaceOne(Expression *pattern, Expression *replacement,
                               bool replaceAll, int sourceLine);
  static Expression *limit(int number, Expression *control);
  static Expression *limit(int number, int sourceLine);
  static Expression *integer(int value);
  static Expression *variable(std::string *var);
  static Expression *current();
  static Expression *stringConst(std::string *constant);
  static Expression *match(Expression *lhs, Expression *rhs, int sourceLine);

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
    RewindN,
    RequiredN,
  };
  enum ExprKind {
    ControlN,
    VariableN,
    IntegerN,
    VarMatchN,
    StringConstN,
    CallN,
    ArgN,
    BinaryN,
    RegExPatternN,
    RegExReferenceN,
  };
  enum WalkResult { StopW, ContinueW, SkipChildrenW, SkipExpressionsW };

  static const char *const CURRENT_LINE_SYM;
  static Expression *checkPattern(Expression *pattern);
};

class Statement : public AST {
  Statement *next = nullptr;

protected:
  Statement(int sourceLine) : AST(sourceLine) {}

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
class Expression : public AST, public Value {
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
    SET_GLOBAL,
  };

  bool isStatement() const override { return false; }
  virtual ExprKind kind() const = 0;
  //  Expression *getNext() const { return (Expression *)next; }
  //  void setNext(Expression *next) { this->next = next; }

  template <typename ACTION> WalkResult walkDown(const ACTION &a);
  template <typename ACTION> WalkResult walkUp(const ACTION &a);
  BinaryP isOp(Operators op);
  static const char *opName(Operators op);
  Value::Kind valueKind();
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
  bool required;

public:
  Expression *pattern;

  enum { NO_LIMIT = -1 };
  Control(StopKind stopKind, Expression *pattern, bool required, int sourceLine)
      : stopKind(stopKind), limit(NO_LIMIT), required(required),
        pattern(pattern) {}
  void setLimit(int num) { limit = num; }
  bool hasLimit() const { return limit > 0; }
  ExprKind kind() const override { return ControlN; }
  bool getReuired() const { return required; }
  int getLimit() const { return limit; }
  StopKind getStopKind() const { return stopKind; }
};
inline Expression *AST::limit(int number, Expression *control) {
  ((Control *)control)->setLimit(number);
  return control;
}
inline Expression *AST::limit(int number, int sourceLine) {
  auto c = new Control(StopAfter, nullptr, false, sourceLine);
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
  Expression *predicate;
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
      : Statement(sourceLine), columns(columns), inExpr(inExpr) {}
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
inline Expression *AST::current() {
  return new Variable(*Symbol::findSymbol(CURRENT_LINE_SYM));
}

class Required : public Statement {
  int count;

public:
  Expression *predicate;
  Required(int count, Expression *predicate, int sourceLine)
      : Statement(sourceLine), count(count), predicate(predicate) {}
  static StmtKind typeKind() { return RequiredN; }
  StmtKind kind() const override { return typeKind(); }
  int getCount() const { return count; }
};

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

class VarMatch : public Integer {
public:
  VarMatch(int value) : Integer(value) {}
  ExprKind kind() const override { return VarMatchN; }
};

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

  Print(Expression *text, Expression *buffer, int sourceLine)
      : Statement(sourceLine), text(text), buffer(buffer) {}
  static StmtKind typeKind() { return PrintN; }
  StmtKind kind() const override { return typeKind(); }
};

class Error : public Statement {

public:
  Expression *text;

  Error(Expression *text, int sourceLine) : Statement(sourceLine), text(text) {}
  static StmtKind typeKind() { return ErrorN; }
  StmtKind kind() const override { return typeKind(); }
};

class IOStmt : public Statement {
protected:
  IOStmt(Expression *buffer, int sourceLine)
      : Statement(sourceLine), buffer(buffer) {}

public:
  Expression *buffer;
};

class Input : public IOStmt {
public:
  Input(Expression *buffer, int sourceLine) : IOStmt(buffer, sourceLine) {}
  static StmtKind typeKind() { return InputN; }
  StmtKind kind() const override { return typeKind(); }
};
class Output : public IOStmt {
public:
  Output(Expression *buffer, int sourceLine) : IOStmt(buffer, sourceLine) {}
  static StmtKind typeKind() { return OutputN; }
  StmtKind kind() const override { return typeKind(); }
};
class Rewind : public IOStmt {
public:
  Rewind(Expression *buffer, int sourceLine) : IOStmt(buffer, sourceLine) {}
  static StmtKind typeKind() { return RewindN; }
  StmtKind kind() const override { return typeKind(); }
};

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
template <typename T> const T *isa(const Statement *stmt) {
  return (stmt->kind() == T::typeKind() ? (T *)stmt : nullptr);
}

// where we compiler a regular expression
class RegExPattern : public Expression {
  int index;

public:
  Expression *pattern;
  RegExPattern(Expression *pattern, int index = -1)
      : Expression(pattern->getSourceLine()), index(index), pattern(pattern) {}
  ExprKind kind() const override { return RegExPatternN; }
  void setIndex(int index) { this->index = index; };
  int getIndex() const { return index; }
};

inline Expression *AST::match(Expression *lhs, Expression *rhs,
                              int sourceLine) {
  auto r = new RegExPattern(rhs);
  return new Binary(Binary::MATCH, lhs, r, sourceLine);
}


// a reference to a patttern (a non-tree link)
class RegExReference : public Expression {
  bool matchAll = false;
public:
  RegExPattern *pattern;
  RegExReference(RegExPattern *pattern)
      : Expression(pattern->getSourceLine()), pattern(pattern) {}
  ExprKind kind() const override { return RegExReferenceN; }
  void setMatchAll(bool matchAll =true) { this->matchAll = matchAll; }
  bool getMatchAll() const { return matchAll; }
};

// TODO -- optimization pass to pull string cmoparison and regular expression
//    compilation out of loops.
//    optimize join reductions
// TODO -- add foreeach split ...
// TODO -- add foreach shell(....), after adding "shell()"
//

#endif /* defined(__rsed__AST__) */
