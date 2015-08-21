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
  AST *next;

public:
  void dump() const;
  void processStrings();

  enum StopKind { StopAfter, StopAt };

  AST(AST *next = nullptr) : next(next) {}

  template <typename T> static T *list(T *car, T *cdr) {
    if (!car)
      return cdr;
    car->next = cdr;
    return car;
  }

  static Statement *replace(std::string *pattern, std::string *replaement);
  static inline Statement *emptyStmt() { return nullptr; }
  static inline Expression *emptyExpr() { return nullptr; }

  static Statement *foreach (Expression *control, Statement * body);
  static Statement *copy(Expression *control);
  static Statement *skip(Expression *control);
  static Statement *skipOne(Expression *pattern);
  static Statement *replace(Expression *control, Expression *pattern,
                            Expression *replacement);
  static Statement *replaceOne(Expression *pattern, Expression *replacement);
  static Statement *ifStmt(Expression *pattern, Statement *thenStmts,
                           Statement *elseStmts);
  static Statement *ifGuard(Expression *pattern, Statement *stmt);
  static Statement *print(Expression *text, Expression *buffer);
  static Statement *error(Expression *text);
  static Statement *input(Expression *buffer);
  static Statement *output(Expression *buffer);

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

  static Expression *fileBuffer(Expression *name);
  static Expression *memoryBuffer(std::string *name);
  static inline Expression *all() { return emptyExpr(); }

  virtual bool isStatement() const = 0;

  enum StmtKind {
    ForeachN,
    SkipN,
    CopyN,
    ReplaceN,
    IfStmtN,
    SetN,
    PrintN,
    ErrorN,
    InputN,
    OutputN,
  };
  enum ExprKind {
    ControlN,
    PatternN,
    NotN,
    VariableN,
    IntegerN,
    BufferN,
    MatchN,
    StringConstN
  };

  enum WalkResult { StopW, ContinueW, SkipChildrenW, SkipExpressionsW };
};

class Statement : public AST {
public:
  bool isStatement() const override { return true; }
  virtual StmtKind kind() const = 0;
  Statement() {}
  Statement *getNext() const { return (Statement *)next; }

  template <typename ACTION> WalkResult walk(const ACTION &a);
  template <typename ACTION> WalkResult walkExprs(const ACTION &a);
};

class Expression : public AST {
public:
  bool isStatement() const override { return false; }
  virtual ExprKind kind() const = 0;
  Expression *getNext() const { return (Expression *)next; }

  template <typename ACTION> WalkResult walkDown(const ACTION &a);
  template <typename ACTION> WalkResult walkUp(const ACTION &a);
};

class Foreach : public Statement {
  Expression *control;
  Statement *body;

public:
  Foreach(Expression *control, Statement *body)
      : control(control), body(body) {}
  StmtKind kind() const override { return ForeachN; }

  Expression *getControl() const { return control; }
  Statement *getBody() const { return body; }
};
inline Statement *AST::foreach (Expression *control, Statement * body) {
  return new Foreach(control, body);
}

class Control : public Expression {
  int limit;
  StopKind stopKind;
  Expression *pattern;

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
  StmtKind kind() const override { return CopyN; }
};
inline Statement *AST::copy(Expression *control) {
  return foreach (control, new Copy());
}

class Skip : public CopySkip {
public:
  StmtKind kind() const override { return SkipN; }
};
inline Statement *AST::skip(Expression *control) {
  return foreach (control, new Skip());
}

class Replace : public Statement {
  Expression *pattern;
  Expression *replacement;

public:
  Replace(Expression *pattern, Expression *replacement)
      : pattern(pattern), replacement(replacement) {}
  StmtKind kind() const override { return ReplaceN; }
  Expression *getPattern() const { return pattern; }
  Expression *getReplacement() const { return replacement; }
};
inline Statement *AST::replace(Expression *control, Expression *pattern,
                               Expression *replacement) {
  return foreach (control, new Replace(pattern, replacement));
}
inline Statement *AST::replaceOne(Expression *pattern,
                                  Expression *replacement) {
  return new Replace(pattern, replacement);
}

class IfStatement : public Statement {
  Expression *pattern;
  Statement *thenStmts;
  Statement *elseStmts;

public:
  IfStatement(Expression *pattern, Statement *thenStmts, Statement *elseStmts)
      : pattern(pattern), thenStmts(thenStmts), elseStmts(elseStmts) {}
  StmtKind kind() const override { return IfStmtN; }
  Expression *getPattern() const { return pattern; }
  Statement *getThenStmts() const { return thenStmts; }
  Statement *getElseStmts() const { return elseStmts; }
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

class Pattern : public Expression {
  StringRef pattern;

public:
  Pattern(std::string *pattern) : pattern(pattern) {}
  ExprKind kind() const override { return PatternN; }
  const StringRef &getPattern() const { return pattern; }
};
inline Expression *AST::pattern(std::string *pattern) {
  return new Pattern(pattern);
}

class NotExpr : public Expression {
  Expression *pattern;

public:
  NotExpr(Expression *pattern) : pattern(pattern) {}
  ExprKind kind() const override { return NotN; }
  Expression *getPattern() const { return pattern; }
};
inline Expression *AST::notExpr(Expression *pattern) {
  return new NotExpr(pattern);
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
  Symbol &symbol;
  Expression *rhs;

public:
  Set(Symbol &symbol, Expression *rhs) : rhs(rhs), symbol(symbol) {}
  StmtKind kind() const override { return SetN; }
  const std::string &getName() const { return symbol.getName(); }
  Symbol &getSymbol() const { return symbol; }
  Expression *getRhs() const { return rhs; }
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
  StmtKind kind() const override { return PrintN; }
  Expression *getText() const { return text; }
  Expression *getBuffer() const { return buffer; }
};
inline Statement *AST::print(Expression *text, Expression *buffer) {
  return new Print(text, buffer);
}

class Error : public Statement {
  Expression *text;

public:
  Error(Expression *text) : text(text) {}
  StmtKind kind() const override { return ErrorN; }
  Expression *getText() const { return text; }
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
  StmtKind kind() const override { return InputN; }
  Expression *getBuffer() const { return buffer; }
};
inline Statement *AST::input(Expression *buffer) { return new Input(buffer); }

class Output : public Statement {
  Expression *buffer;

public:
  Output(Expression *buffer) : buffer(buffer) {}
  StmtKind kind() const override { return OutputN; }
  Expression *getBuffer() const { return buffer; }
};
inline Statement *AST::output(Expression *buffer) { return new Output(buffer); }

class Match : public Expression {
  Expression *pattern;
  Expression *target;

public:
  Match(Expression *pattern, Expression *target)
      : pattern(pattern), target(target) {}
  ExprKind kind() const override { return MatchN; }
  Expression *getPattern() const { return pattern; }
  Expression *getTarget() const { return target; }
};
inline Expression *AST::match(Expression *pattern, Expression *target) {
  return new Match(pattern, target);
}

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
      rc = ((Foreach *)s)->getBody()->walk(a);
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
    rc = ((Replace *)this)->getPattern()->walkDown(a);
    if (rc != ContinueW)
      return rc;
    rc = ((Replace *)this)->getReplacement()->walkDown(a);
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
  }

  return rc;
}

template <typename ACTION> AST::WalkResult Expression::walkUp(const ACTION &a) {
  Expression *e = this;
  for (; e; e = e->getNext()) {
    WalkResult rc = ContinueW;
    switch (e->kind()) {
    case ControlN:
      rc = ((Control *)e)->getPattern()->walkUp(a);
      if (rc != ContinueW)
        return rc;
      break;
    case MatchN:
      rc = walkUp(((Match *)e)->getPattern(), a);
      if (rc != ContinueW)
        return rc;
      rc = walkUp(((Match *)e)->getTarget(), a);
      if (rc != ContinueW)
        return rc;
      break;
    case NotN:
      rc = walkUp(((NotExpr *)e)->getPattern(), a);
      if (rc != ContinueW)
        return rc;
      break;
    default:
      break;
    }
    rc = a(e);
    if (rc != ContinueW)
      return rc;
  }
  return ContinueW;
}

template <typename ACTION>
AST::WalkResult Expression::walkDown(const ACTION &a) {
  Expression *e = this;
  for (; e; e = e->getNext()) {
    WalkResult rc = a(e);
    if (rc != ContinueW)
      return rc;

    switch (e->kind()) {
    case ControlN:
      rc = ((Control *)e)->getPattern()->walkDown(a);
      if (rc != ContinueW)
        return rc;
      break;
    case MatchN:
      rc = ((Match *)e)->getPattern()->walkDown(a);
      if (rc != ContinueW)
        return rc;
      rc = ((Match *)e)->getTarget()->walkDown(a);
      if (rc != ContinueW)
        return rc;
      break;
    case NotN:
      rc = ((NotExpr *)e)->getPattern()->walkDown(a);
      if (rc != ContinueW)
        return rc;
      break;
    default:
      break;
    }
  }
  return ContinueW;
}

#endif /* defined(__rsed__AST__) */
