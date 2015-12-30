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
#include <vector>
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
  static Expression *number(double value);
  static Expression *variable(std::string *var);
  static Expression *current();
  static Expression *stringConst(std::string *constant);
  static Expression *match(Expression *lhs, Expression *rhs, int sourceLine);
  static Statement *input(Expression *source, int sourceLine);
  static Statement *input(Statement *);

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
    SplitInputN,
    ColumnsInputN,
    OutputN,
    CloseN,
    RequiredN,
    HoistedValueN,
  };
  enum ExprKind {
    ControlN,
    VariableN,
    NumberN,
    VarMatchN,
    StringConstN,
    CallN,
    ArgN,
    BinaryN,
    RegExPatternN,
    HoistedValueRefN,
  };
  enum WalkResult { StopW, ContinueW, SkipChildrenW, SkipExpressionsW };

  static const char *const CURRENT_LINE_SYM;
  static Expression *checkPattern(Expression *pattern);
  virtual ~AST() {}
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
  template <typename ACTION> void applyExprs(const ACTION &);
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
template <typename T> T *isa(Statement *stmt) {
  return (stmt->kind() == T::typeKind() ? (T *)stmt : nullptr);
}
template <typename T> const T *isa(const Statement *stmt) {
  return (stmt->kind() == T::typeKind() ? (T *)stmt : nullptr);
}

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

  template <typename ACTION> WalkResult walkDown(const ACTION &a);
  template <typename ACTION> WalkResult walkUp(const ACTION &a);
  template <typename ACTION>
  void walkConcat(const ACTION &action,
                  std::vector<class Binary *> *operators = nullptr);
  BinaryP isOp(Operators op);
  const BinaryP isOp(Operators op) const;
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
  bool isOp(Operators op) const { return op == this->op; }
};
typedef Binary *BinaryP;
inline BinaryP Expression::isOp(Operators op) {
  return (kind() == BinaryN && BinaryP(this)->isOp(op) ? BinaryP(this)
                                                       : nullptr);
}
inline const BinaryP Expression::isOp(Operators op) const {
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
  Copy(int sourceLine) : CopySkip(sourceLine) {}
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
  Replace(Expression *pattern, Expression *replacement, int sourceLine)
      : Statement(sourceLine), pattern(pattern), replacement(replacement) {}
  static StmtKind typeKind() { return ReplaceN; }
  StmtKind kind() const override { return typeKind(); }
};
class Split : public Statement {
public:
  Expression *separator;
  Expression *target;
  Split(Expression *separator, Expression *target, int sourceLine)
      : Statement(sourceLine), separator(separator), target(target) {}
  static StmtKind typeKind() { return SplitN; }
  StmtKind kind() const override { return typeKind(); }
};
class SplitInput : public Split {
public:
  SplitInput(Expression *columns, Expression *inExpr, int sourceLine)
      : Split(columns, inExpr, sourceLine) {}
  static StmtKind typeKind() { return SplitInputN; }
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
class ColumnsInput : public Columns {
public:
  ColumnsInput(Expression *columns, Expression *inExpr, int sourceLine)
      : Columns(columns, inExpr, sourceLine) {}
  static StmtKind typeKind() { return ColumnsInputN; }
  StmtKind kind() const override { return typeKind(); }
};
inline Statement *AST::input(Statement *s) {
  Statement *result;
  if (auto c = isa<Columns>(s)) {
    result = new ColumnsInput(c->columns, c->inExpr, c->getSourceLine());
  } else {
    auto sp = isa<Split>(s);
    result = new SplitInput(sp->separator, sp->target, sp->getSourceLine());
  }
  delete s;
  return result;
}

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
  StringConst(StringRef constant) : constant(constant) {}
  StringConst(const char *constant) : constant(constant) {}
  ExprKind kind() const override { return StringConstN; }
  const StringRef &getConstant() const { return constant; }
  void setConstant(StringRef constant) { this->constant = constant; }
};

class Number : public Expression {
  double value;

public:
  Number(double value) : value(value) {}
  ExprKind kind() const override { return NumberN; }
  double getValue() const { return value; }
};
inline Expression *AST::number(double value) { return new Number(value); }

class VarMatch : public Number {
public:
  VarMatch(int value) : Number(value) {}
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
  bool shellCmd = false;

public:
  Input(Expression *buffer, bool shellCmd, int sourceLine)
      : IOStmt(buffer, sourceLine), shellCmd(shellCmd) {}
  static StmtKind typeKind() { return InputN; }
  StmtKind kind() const override { return typeKind(); }
  bool getShellCmd() const { return shellCmd; }
  void setShellCmd(bool shellCmd) { this->shellCmd = shellCmd; }
};

class Output : public IOStmt {
public:
  Output(Expression *buffer, int sourceLine) : IOStmt(buffer, sourceLine) {}
  static StmtKind typeKind() { return OutputN; }
  StmtKind kind() const override { return typeKind(); }
};
class Close : public IOStmt {
public:
  enum Mode {
    Input,
    Output,
    ByName,
  };
  Close(Expression *buffer, Mode mode, int sourceLine)
      : IOStmt(buffer, sourceLine), mode(mode) {}
  static StmtKind typeKind() { return CloseN; }
  StmtKind kind() const override { return typeKind(); }
  Mode getMode() const { return mode; }

private:
  enum Mode mode;
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

// where we compiler a regular expression
class RegExPattern : public Expression {
  int index;
  static int nextIndex;

public:
  Expression *pattern;
  RegExPattern(Expression *pattern, int index = -1)
      : Expression(pattern->getSourceLine()), index(nextIndex++),
        pattern(pattern) {}
  ExprKind kind() const override { return RegExPatternN; }
  void setIndex(int index) { this->index = index; };
  int getIndex() const { return index; }
};

inline Expression *AST::match(Expression *lhs, Expression *rhs,
                              int sourceLine) {
  auto r = new RegExPattern(rhs);
  return new Binary(Binary::MATCH, lhs, r, sourceLine);
}

inline Statement *AST::replace(Expression *control, Expression *pattern,
                               Expression *replacement, bool optAll,
                               int sourceLine) {
  if (optAll) {
    pattern = new Binary(Binary::SET_GLOBAL, nullptr, pattern, sourceLine);
  }
  pattern = new RegExPattern(pattern);
  Statement *body = new Replace(pattern, replacement, sourceLine);
#ifndef IMPLICIT_FOREACH_COPY
  body = Statement::list<Statement>(body, new Copy(sourceLine));
#endif
  return new Foreach(control, body, sourceLine);
}
inline Statement *AST::replaceOne(Expression *pattern, Expression *replacement,
                                  bool optAll, int sourceLine) {
  if (optAll) {
    pattern = new Binary(Binary::SET_GLOBAL, nullptr, pattern, sourceLine);
  }
  pattern = new RegExPattern(pattern);
  return new Replace(pattern, replacement, sourceLine);
}

class HoistedValue : public Statement {
public:
  Expression *rhs;
  HoistedValue(Expression *rhs) : Statement(rhs->getSourceLine()), rhs(rhs) {}
  static StmtKind typeKind() { return HoistedValueN; }
  StmtKind kind() const override { return typeKind(); }
};

class HoistedValueRef : public Expression {
public:
  Expression *value;
  HoistedValueRef(Expression *value)
      : Expression(value->getSourceLine()), value(value) {}
  ExprKind kind() const override { return HoistedValueRefN; }
};

// TODO -- optimize join reductions
//       append x to y [with sep]
//       $? -- number of matches?
// TODO -- think about adding lists [x,y,z]
//      with iteration over lists, functions of lists,
//      implicit concatenation when used like a string
//      subscript operator  x[i]
//
// TODO  split ... into var1, ..., vk
//    or (v1,...vk) = split ...
// TODO why aren't we processing regex escapes?
// TODO add support for floating point values in the input
// TODO add true/false as boolean literals
// TODO NaN should coerce to false
// TODO add number(), string(), logical() explicit coerion functions
// TODO is it a good idea for not e to be implicitly not match e?
//       maybe a string-boolean converstion should be via match?
// TODO add a mechanism to print a string without a newline (WRITE, PRINT STRING, ...)


#endif /* defined(__rsed__AST__) */
