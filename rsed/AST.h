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
  static Statement *skip();
  static Statement *skipOne(Expression *pattern);
  static Statement *replace(Expression *control, Expression *pattern,
                            Expression *replacement, bool replaceAll,
                            int sourceLine);
  static Statement *replaceOne(Expression *pattern, Expression *replacement,
                               bool replaceAll, int sourceLine);
  static Statement *ifStmt(Expression *pattern, Statement *thenStmts,
                           Statement *elseStmts);
  static Statement *ifGuard(Expression *pattern, Statement *stmt);
  static Statement *print(Expression *text, Expression *buffer);
  static Statement *error(Expression *text);
  static Statement *input(Expression *buffer);
  static Statement *output(Expression *buffer);
  static Statement *columns(Expression *cols, Expression *inExpr);

  static Expression *limit(int number, Expression *control);
  static Expression *limit(int number);
  static Expression *control(StopKind stopKind, Expression *pattern);
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
    OR
  };
  static const char *opName(Operators op);
  enum WalkResult { StopW, ContinueW, SkipChildrenW, SkipExpressionsW };
};

class Statement : public AST {
  Statement *next = nullptr;

public:
  Statement(int sourceLine = 0) : AST(sourceLine) {}
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

class Expression : public AST {
protected:
  Expression(int sourceLine = 0) : AST(sourceLine) {}

public:
  bool isStatement() const override { return false; }
  virtual ExprKind kind() const = 0;
  //  Expression *getNext() const { return (Expression *)next; }
  //  void setNext(Expression *next) { this->next = next; }

  template <typename ACTION> WalkResult walkDown(const ACTION &a);
  template <typename ACTION> WalkResult walkUp(const ACTION &a);
  bool isOp(AST::Operators op);
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
inline bool Expression::isOp(AST::Operators op) {
  return kind() == BinaryN && BinaryP(this)->isOp(op);
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
  Expression *pattern;
  int limit;

public:
  enum { NO_LIMIT = -1 };
  Control(StopKind stopKind, Expression *pattern)
      : stopKind(stopKind), pattern(pattern), limit(NO_LIMIT) {}
  void setLimit(int num) { limit = num; }
  bool hasLimit() const { return limit > 0; }
  ExprKind kind() const override { return ControlN; }

  int getLimit() const { return limit; }
  StopKind getStopKind() const { return stopKind; }
  Expression *getPattern() const { return pattern; }
  void setPattern(Expression *pattern) { this->pattern = pattern; }
};
inline Expression *AST::control(AST::StopKind stopKind, Expression *pattern) {
  return new Control(stopKind, pattern);
}
inline Expression *AST::limit(int number, Expression *control) {
  ((Control *)control)->setLimit(number);
  return control;
}
inline Expression *AST::limit(int number) {
  auto c = new Control(StopAfter, nullptr);
  c->setLimit(number);
  return c;
}

class CopySkip : public Statement {
public:
};
class Copy : public CopySkip {
public:
  static StmtKind typeKind() { return CopyN; }
  StmtKind kind() const override { return typeKind(); }
};
inline Statement *AST::copy(Expression *control, int sourceLine) {
  return new Foreach(control, new Copy(), sourceLine);
}

class Skip : public CopySkip {
public:
  static StmtKind typeKind() { return SkipN; }
  StmtKind kind() const override { return typeKind(); }
};
inline Statement *AST::skip(Expression *control, int sourceLine) {
  return new Foreach(control, new Skip(), sourceLine);
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
  body = Statement::list<Statement>(body, new Copy());
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
  Expression *pattern; // TODO rename predicate
  Statement *thenStmts;
  Statement *elseStmts;

public:
  IfStatement(Expression *pattern, Statement *thenStmts, Statement *elseStmts)
      : pattern(pattern), thenStmts(thenStmts), elseStmts(elseStmts) {}
  static StmtKind typeKind() { return IfStmtN; }
  StmtKind kind() const override { return typeKind(); }
  Expression *getPattern() const { return pattern; }
  void setPattern(Expression *pattern) { this->pattern = pattern; }
  Statement *getThenStmts() const { return thenStmts; }
  void setThenStmts(Statement *thenStmts) { this->thenStmts = thenStmts; }
  Statement *getElseStmts() const { return elseStmts; }
  void setElseStmts(Statement *elseStmts) { this->elseStmts = elseStmts; }
};
inline Statement *AST::ifStmt(Expression *pattern, Statement *thenStmts,
                              Statement *elseStmts) {
  return new IfStatement(pattern, thenStmts, elseStmts);
}
inline Statement *AST::ifGuard(Expression *pattern, Statement *stmt) {
  return new IfStatement(pattern, stmt, nullptr);
}
inline Statement *AST::skipOne(Expression *pattern) {
  return new IfStatement(pattern, new Skip(), nullptr);
}
inline Statement *AST::skip() { return new Skip(); }

class Columns : public Statement {
  Expression *columns;
  Expression *inExpr;

public:
  Columns(Expression *columns, Expression *inExpr)
      : columns(columns), inExpr(inExpr) {}
  static StmtKind typeKind() { return ColumnsN; }
  StmtKind kind() const override { return typeKind(); }
  Expression *getColumns() const { return columns; }
  void setColumns(Expression *columns) { this->columns = columns; }
  Expression *getInExpr() const { return inExpr; }
  void setInExpr(Expression *inExpr) { this->inExpr = inExpr; }
};
inline Statement *AST::columns(Expression *cols, Expression *inExpr) {
  return new Columns(cols, inExpr);
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

class StringConst : public Expression {
  StringRef constant;

public:
  StringConst(std::string *constant) : constant(constant) {}
  ExprKind kind() const override { return StringConstN; }
  const StringRef &getConstant() const { return constant; }
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
  Expression *rhs;
  Symbol &symbol;

public:
  Set(Symbol &symbol, Expression *rhs) : rhs(rhs), symbol(symbol) {}
  static StmtKind typeKind() { return SetN; }
  StmtKind kind() const override { return typeKind(); }
  const std::string &getName() const { return symbol.getName(); }
  Symbol &getSymbol() const { return symbol; }
  Expression *getRhs() const { return rhs; }
  void setRhs(Expression *rhs) { this->rhs = rhs; }
};
inline Statement *AST::set(std::string *lhs, Expression *stringExpr) {
  auto s = new Set(*Symbol::findSymbol(*lhs), stringExpr);
  delete lhs;
  return s;
}

class Print : public Statement {
  Expression *text;
  Expression *buffer;

public:
  Print(Expression *text, Expression *buffer) : text(text), buffer(buffer) {}
  static StmtKind typeKind() { return PrintN; }
  StmtKind kind() const override { return typeKind(); }
  Expression *getText() const { return text; }
  void setText(Expression *text) { this->text = text; }
  Expression *getBuffer() const { return buffer; }
  void setBuffer(Expression *buffer) { this->buffer = buffer; }
};
inline Statement *AST::print(Expression *text, Expression *buffer) {
  return new Print(text, buffer);
}

class Error : public Statement {
  Expression *text;

public:
  Error(Expression *text) : text(text) {}
  static StmtKind typeKind() { return ErrorN; }
  StmtKind kind() const override { return typeKind(); }
  Expression *getText() const { return text; }
  void setText(Expression *text) { this->text = text; }
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
  Expression *buffer;

public:
  Input(Expression *buffer) : buffer(buffer) {}
  static StmtKind typeKind() { return InputN; }
  StmtKind kind() const override { return typeKind(); }
  Expression *getBuffer() const { return buffer; }
};
inline Statement *AST::input(Expression *buffer) { return new Input(buffer); }

class Output : public Statement {
  Expression *buffer;

public:
  Output(Expression *buffer) : buffer(buffer) {}
  static StmtKind typeKind() { return OutputN; }
  StmtKind kind() const override { return typeKind(); }
  Expression *getBuffer() const { return buffer; }
};
inline Statement *AST::output(Expression *buffer) { return new Output(buffer); }

#if 0
class RegEx;
class Match : public Expression {
  Expression *target;
  Expression *pattern;
  RegEx *regEx = nullptr;

public:
  Match(Expression *target, Expression *pattern)
      : target(target), pattern(pattern) {}
  ExprKind kind() const override { return MatchN; }
  Expression *getPattern() const { return pattern; }
  void setPattern(Expression *pattern) { this->pattern = pattern; }
  Expression *getTarget() const { return target; }
  void setTarget(Expression *target) { this->target = target; }
  RegEx *getRegEx() const { return regEx; }
  void setRegEx(RegEx *regEx) { this->regEx = regEx; }
};
inline Expression *AST::match(Expression *pattern, Expression *target) {
  return new Match(pattern, target);
}
#endif

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

template <typename ACTION> AST::WalkResult Statement::walk(const ACTION &a) {
  Statement *s = this;
  for (; s; s = s->getNext()) {
    auto rc = a(s);
    if (rc == StopW)
      return rc;
    if (rc == SkipChildrenW)
      continue;

    switch (s->kind()) {
    case ForeachN: {
      rc = ForeachP(s)->body->walk(a);
      if (rc == StopW)
        return rc;
      break;
    }

    case IfStmtN: {
      auto ifs = (IfStatement *)s;
      rc = ifs->getThenStmts()->walk(a);
      if (rc == StopW)
        return rc;
      if (rc == ContinueW) {
        if (auto elseStmts = ifs->getElseStmts()) {
          rc = elseStmts->walk(a);
          if (rc == StopW)
            return rc;
        }
      }
    }
    default:
      break;
    }
  }
  return ContinueW;
}

template <typename ACTION>
AST::WalkResult Statement::walkExprs(const ACTION &a) {
  auto rc = ContinueW;
  switch (kind()) {
  case ControlN:
    return ((Control *)this)->getPattern()->walkDown(a);
  case InputN:
    return ((Input *)this)->getBuffer()->walkDown(a);
  case OutputN:
    return ((Output *)this)->getBuffer()->walkDown(a);
  case PrintN:
    rc = ((Print *)this)->getText()->walkDown(a);
    if (rc == ContinueW) {
      if (auto buffer = ((Print *)this)->getBuffer()) {
        rc = buffer->walkDown(a);
      }
    }
    break;
  case CopyN:
  case SkipN:
    break;
  case ReplaceN:
    rc = ((Replace *)this)->pattern->walkDown(a);
    if (rc != ContinueW)
      return rc;
    rc = ((Replace *)this)->replacement->walkDown(a);
    break;
  case SplitN:
    if ((Split *)this->pattern) {
      rc = ((Split *)this)->separator->walkDown(a);
      if (rc != ContinueW)
        return rc;
    }
    rc = ((Split *)this)->separator->walkDown(a);
    break;
  case SetN:
    rc = ((Set *)this)->getRhs()->walkDown(a);
    break;
  case IfStmtN:
    rc = ((IfStatement *)this)->getPattern()->walkDown(a);
    break;
  case ErrorN:
    rc = ((Error *)this)->getText()->walkDown(a);
    break;
  case ColumnsN:
    rc = ((Columns *)this)->getColumns()->walkDown(a);
    break;
  }

  return rc;
}

template <typename ACTION> AST::WalkResult Expression::walkUp(const ACTION &a) {
  Expression *e = this;
  WalkResult rc = ContinueW;
  switch (e->kind()) {
  case ControlN:
    rc = ((Control *)e)->getPattern()->walkUp(a);
    break;
  case BinaryN:
    if (BinaryP(e)->left) {
      rc = walkUp(BinaryP(e)->left);
      if (rc != ContinueW)
        return rc;
    }
    rc = walkUp(BinaryP(e)->right);
    break;
  case CallN:
    for (auto arg = CallP(e)->args; arg; arg = arg->nextArg) {
      rc = arg->value->walkUp(a);
      if (rc != ContinueW)
        return rc;
    }
    break;
  default:
    rc = ContinueW;
    break;
  }
  if (rc != ContinueW)
    return rc;
  rc = a(e);
  if (rc != ContinueW)
    return rc;
  return ContinueW;
}

template <typename ACTION>
AST::WalkResult Expression::walkDown(const ACTION &a) {
  Expression *e = this;
  WalkResult rc = a(e);
  if (rc != ContinueW) {
    if (rc == SkipChildrenW)
      rc = ContinueW;
    return rc;
  }

  switch (e->kind()) {
  case ControlN:
    rc = ((Control *)e)->getPattern()->walkDown(a);
    if (rc != ContinueW)
      return rc;
    break;
  case BinaryN:
    rc = BinaryP(e)->left->walkDown(a);
    if (rc != ContinueW)
      return rc;
    rc = BinaryP(e)->right->walkDown(a);
    if (rc != ContinueW)
      return rc;
    break;
  case CallN:
    for (auto arg = CallP(e)->args; arg; arg = arg->nextArg) {
      rc = arg->value->walkDown(a);
      if (rc != ContinueW)
        return rc;
    }
    break;

  default:
    break;
  }
  return ContinueW;
}

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
