//
//  Interpreter.cpp
//  rsed
//
//  Created by David Callahan on 6/27/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include <assert.h>
#include <sstream>
#include <iostream>
#include "Symbol.h"
#include "AST.h"
#include "Interpreter.h"
#include "LineBuffer.h"
#include "RegEx.h"

enum ResultCode {
  OK_S,   // continue
  NEXT_S, // skip to next line
  STOP_S, // stop processing
};
enum MatchKind { NoMatchK, StopAtK, StopAfterK };

class State {

public:
  bool saw_error;
  RegEx *regEx;
  std::string inputLine;
  std::string currentLine;
  LineBuffer *inputBuffer;
  bool inputEof;
  void nextLine() {
    if (!inputEof) {
      inputEof = !inputBuffer->getLine(inputLine);
      currentLine = inputLine;
    }
  }
  LineBuffer *outputBuffer;

  ResultCode interpret(Statement *stmtList);
  ResultCode interpretOne(Statement *stmt);
  ResultCode interpret(Foreach *foreach);
  ResultCode interpret(IfStatement *ifstmt);
  std::string eval(Expression *ast);
  StringRef evalPattern(Expression *ast);
  // evaluate match control against current linke
  MatchKind matchPattern(AST &);
};

class ForeachControl {
  State *state;
  int regIndex;
  bool negate;
  MatchKind matchKind;
  bool hasCount;
  unsigned count;
  bool all;

public:
  ForeachControl(State *state)
      : state(state), regIndex(-1), negate(false), matchKind(NoMatchK),
        hasCount(true), count(0), all(false) {}
  bool initialize(Control *c);
  MatchKind eval(const std::string &line);
  void release() {
    if (regIndex >= 0) {
      state->regEx->releasePattern(regIndex);
      regIndex = -1;
    }
  }
  ~ForeachControl() { release(); }
};

MatchKind ForeachControl::eval(const std::string &line) {
  if (all) {
    return NoMatchK;
  }
  if (hasCount) {
    if (count == 0)
      return StopAtK;
    count -= 1;
  }
  bool rc = state->regEx->match(regIndex, line);
  if (rc ^ negate) {
    return matchKind;
  }
  return NoMatchK;
}

bool ForeachControl::initialize(Control *c) {
  if (!c) {
    all = true;
    return true;
  }
  hasCount = c->hasLimit();
  if (hasCount) {
    count = (unsigned)c->getLimit();
  }
  matchKind = (c->getStopKind() == AST::StopAt ? StopAtK : StopAfterK);
  auto p = c->getPattern();
  if (p->kind() == AST::NotN) {
    negate = true;
    p = ((NotExpr *)p)->getPattern();
  }
  StringRef pattern = state->evalPattern(p);
  regIndex = state->regEx->setPattern(pattern);
  return regIndex >= 0;
}

std::string State::eval(Expression *e) {
  std::stringstream buffer;
  e->walkDown([&buffer, this](Expression *e) {

    switch (e->kind()) {
    case AST::StringConstN: {
      buffer << ((StringConst *)e)->getConstant().text;
      break;
    }
    case AST::IntegerN:
      buffer << ((Integer *)e)->getValue();
      break;
    case AST::VariableN: {
      buffer << ((Variable *)e)->getSymbol().getValue();
      break;
    }
    case AST::BufferN:
    case AST::ControlN:
    case AST::PatternN:
    case AST::NotN:
    case AST::MatchN:
      assert(!"unexpected node kind in expression eval");
      break;
    }
    return AST::ContinueW;

  });
  std::string result;
  buffer >> result;
  return result;
}

// this expression is:
StringRef State::evalPattern(Expression *ast) {
  std::stringstream s;
  unsigned flags = 0;
  ast->walkDown([&s, &flags, this](Expression *e) {
    switch (e->kind()) {
    case AST::StringConstN: {
      const auto &sc = ((StringConst *)e)->getConstant();
      s << sc;
      flags |= sc.flags;
      break;
    }
    case AST::VariableN:
      s << ((Variable *)e)->getSymbol().getValue();
      break;
    case AST::IntegerN:
      s << ((Integer *)e)->getValue();
      break;
    case AST::BufferN:
    case AST::ControlN:
    case AST::PatternN:
    case AST::NotN:
    case AST::MatchN:
      assert(!"invalid expresion in pattern");
    }
    return AST::ContinueW;
  });
  StringRef result;
  s >> result.text;
  result.flags = flags;
  return result;
}

ResultCode State::interpret(Statement *stmtList) {
  ResultCode rc = OK_S;
  stmtList->walk([this, &rc](Statement *stmt) {
    rc = interpretOne(stmt);
    return (rc == OK_S ? AST::ContinueW : AST::StopW);
  });
  return rc;
}

ResultCode State::interpret(Foreach *foreach) {
  auto c = (Control *)foreach->getControl();
  ForeachControl fc(this);
  if (!fc.initialize(c)) {
    return STOP_S;
  }

  auto b = foreach->getBody();
  while (!inputEof) {
    auto mk = fc.eval(currentLine);
    if (mk == StopAtK) {
      break;
    }
    auto rc = interpret(b);
    if (rc == STOP_S)
      return STOP_S;
    if (mk == StopAfterK) {
      break;
    }
    nextLine();
  }
  return OK_S;
}

ResultCode State::interpret(IfStatement *ifstmt) {
  auto p = ifstmt->getPattern();
  int rc;
  if (p->kind() == AST::MatchN) {
    auto m = (Match *)p;
    StringRef pattern = evalPattern(m->getPattern());
    std::string target = eval(m->getTarget());
    rc = regEx->match(pattern, target);
  } else {
    StringRef pattern = evalPattern(p);
    rc = regEx->match(pattern, currentLine);
  }
  if (rc < 0) {
    return STOP_S;
  }
  if (rc > 0) { // true
    return interpret(ifstmt->getThenStmts());
  }
  if (auto e = ifstmt->getElseStmts()) {
    return interpret(e);
  }
  return OK_S;
}

ResultCode State::interpretOne(Statement *stmt) {
  switch (stmt->kind()) {
  case AST::SkipN:
    return NEXT_S;
  case AST::CopyN:
    outputBuffer->append(currentLine);
    return NEXT_S;
  case AST::PrintN: {
    auto p = (Print *)stmt;
    LineBuffer *out = outputBuffer;
    std::string value = eval(p->getText());
    if (auto b = p->getBuffer()) {
      out = LineBuffer::findOutputBuffer(eval(b));
    }
    out->append(value);
    break;
  }
  case AST::ErrorN: {
    break;
  }
  case AST::ReplaceN: {
    auto r = (Replace *)stmt;
    auto pattern = evalPattern(r->getPattern());
    auto index = regEx->setPattern(pattern);
    if (index < 0)
      return STOP_S;

    auto target = eval(r->getReplacement());
    currentLine = regEx->replace(index, target, currentLine);
    regEx->releasePattern(index);
    break;
  }
  case AST::ForeachN:
    return interpret((Foreach *)stmt);
  case AST::IfStmtN:
    return interpret((IfStatement *)stmt);
  case AST::SetN:
  case AST::InputN:
  case AST::OutputN:
    assert(!"not yet implemented");
    return STOP_S;
  }
  return OK_S;
}

void Interpreter::initialize() {
  state = new State;
  state->inputBuffer = makeInBuffer(&std::cin);
  state->outputBuffer = makeOutBuffer(&std::cout);
  state->regEx = RegEx::regEx;
  Symbol *lineno = makeSymbol(std::string("LINE"), [this]() {
    std::stringstream buf;
    buf << state->inputBuffer->getLineno();
    std::string result;
    buf >> result;
    return result;
  });
  Symbol::defineSymbol(lineno);
}

bool Interpreter::setInput(const std::string &fileName) {
  auto f = new std::ifstream(fileName);
  if (!f || !f->is_open()) {
    std::cerr << "unable to open: " << fileName << '\n';
    return false;
  }
  state->inputBuffer = makeInBuffer(f);
  return true;
}

void Interpreter::interpret(Statement *script) {
  state->nextLine();
  state->interpret(script);
}