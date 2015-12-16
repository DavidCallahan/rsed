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
#include <vector>
#include "Symbol.h"
#include "AST.h"
#include "Interpreter.h"
#include "LineBuffer.h"
#include "RegEx.h"
#include "BuiltinCalls.h"
#include "EvalState.h"

using std::vector;
using std::string;
using std::stringstream;
extern int debug;

enum ResultCode {
  OK_S,   // continue
  NEXT_S, // skip to next line
  STOP_S, // stop processing
};
enum MatchKind { NoMatchK, StopAtK, StopAfterK };

class State : public EvalState {

public:
  bool firstLine = true;
  bool sawError = false;

  // use columns are match for $1, $2,...
  bool matchColumns = true;
  vector<string> columns;

  string inputLine;
  string currentLine_;
  string &getCurrentLine() {
    if (firstLine) {
      nextLine();
    }
    return currentLine_;
  }

  string match(unsigned i) {
    if (matchColumns) {
      if (i >= columns.size()) {
        return "";
      }
      return columns[i];
    } else {
      return regEx->getSubMatch(i);
    }
  }
  LineBuffer *inputBuffer;
  bool inputEof_ = false;
  bool getInputEof() {
    if (firstLine) {
      nextLine();
    }
    return inputEof_;
  }
  void nextLine() {
    if (!inputEof_) {
      inputEof_ = !inputBuffer->getLine(inputLine);
      currentLine_ = inputLine;
      if (debug) {
        std::cout << "input: " << currentLine_ << "\n";
      }
      firstLine = false;
    }
  }
  unsigned getLineno() const override { return inputBuffer->getLineno(); }
  LineBuffer *outputBuffer;

  ResultCode interpret(Statement *stmtList);
  ResultCode interpretOne(Statement *stmt);
  ResultCode interpret(Foreach *foreach);
  ResultCode interpret(IfStatement *ifstmt);
  ResultCode interpret(Set *set);
  ResultCode interpret(Columns *cols);
  string eval(Expression *ast);
  string expandVariables(string text);
  StringRef evalPattern(Expression *ast);
  string evalCall(Call *);
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
  MatchKind eval(const string *line);
  void release() {
    if (regIndex >= 0) {
      state->getRegEx()->releasePattern(regIndex);
      regIndex = -1;
    }
  }
  bool needsInput() const { return all || regIndex >= 0; }
  ~ForeachControl() { release(); }
};

MatchKind ForeachControl::eval(const string *line) {
  if (all) {
    return NoMatchK;
  }
  if (hasCount) {
    if (count == 0)
      return StopAtK;
    count -= 1;
  }
  if (regIndex >= 0) {
    bool rc = state->getRegEx()->match(regIndex, *line);
    state->matchColumns = false;
    if (rc ^ negate) {
      return matchKind;
    }
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
  regIndex = -1;
  if (auto p = c->getPattern()) {
    if (p->kind() == AST::NotN) {
      negate = true;
      p = ((NotExpr *)p)->getPattern();
    }
    StringRef pattern = state->evalPattern(p);
    regIndex = state->getRegEx()->setPattern(pattern);
  }
  return true;
}

string State::eval(Expression *e) { return evalPattern(e).getText(); }

// this expression is:
StringRef State::evalPattern(Expression *ast) {
  stringstream s;
  unsigned flags = 0;
  ast->walkDown([&s, &flags, this](Expression *e) {
    switch (e->kind()) {
    case AST::StringConstN: {
      const auto &sc = ((StringConst *)e)->getConstant();
      s << (sc.isRaw() ? sc.getText() : expandVariables(sc.getText()));
      flags |= sc.getFlags();
      break;
    }
    case AST::VariableN:
      s << ((Variable *)e)->getSymbol().getValue();
      break;
    case AST::IntegerN:
      s << ((Integer *)e)->getValue();
      break;
    case AST::CallN:
      s << evalCall((Call *)e);
      break;
    case AST::BinaryN:
      assert(BinaryP(e)->op == Binary::CONCAT);
      break;
    case AST::BufferN:
    case AST::ControlN:
    case AST::NotN:
    case AST::MatchN:
    case AST::ArgN:
      assert(!"invalid expresion in pattern");
    }
    return AST::ContinueW;
  });
  return StringRef(s.str(), flags);
}

static std::regex variable(R"((\\\$)|(\$[0-9a-zA-Z]+))");

string State::expandVariables(string text) {

  const char *ctext = text.c_str();
  size_t len = text.length();
  auto vars_begin = std::cregex_iterator(ctext, ctext + len, variable);
  auto vars_end = std::cregex_iterator();
  if (vars_begin == vars_end) {
    return text;
  }

  stringstream result;
  const char *last = ctext;
  for (std::cregex_iterator vars = vars_begin; vars != vars_end; ++vars) {
    auto match = (*vars)[0];
    const char *cvar = match.first;
    result.write(last, cvar - last);

    if (*cvar == '\\') {
      result << '$';
    } else {
      result << Symbol::findSymbol(string(cvar + 1, match.second - cvar - 1))
                    ->getValue();
    }
    last = match.second;
  }
  result.write(last, len - (last - ctext));
  return result.str();
}

/// interpret all statements on a list until an error
/// is seen or som
ResultCode State::interpret(Statement *stmtList) {
  ResultCode rc = OK_S;
  stmtList->walk([this, &rc](Statement *stmt) {
    rc = interpretOne(stmt);
    return (rc == OK_S && !sawError ? AST::SkipChildrenW : AST::StopW);
  });
  return (sawError ? STOP_S : rc);
}

ResultCode State::interpret(Foreach *foreach) {
  auto c = (Control *)foreach->control;
  ForeachControl fc(this);
  if (!fc.initialize(c)) {
    return STOP_S;
  }

  auto b = foreach->body;
  for (;;) {
    string *line = nullptr;
    if (fc.needsInput()) {
      if (getInputEof()) {
        break;
      }
      line = &getCurrentLine();
    }
    auto mk = fc.eval(line);
    if (mk == StopAtK) {
      break;
    }
    auto rc = interpret(b);
    if (rc == STOP_S) {
      return STOP_S;
    }
#ifdef IMPLICIT_FOREACH_COPY
    // should there be an implicit copy or no?
    if (rc != NEXT_S) {
      outputBuffer->append(getCurrentLine());
    }
#endif
    nextLine();
    if (mk == StopAfterK) {
      break;
    }
  }
  return OK_S;
}

ResultCode State::interpret(IfStatement *ifstmt) {
  auto p = ifstmt->getPattern();
  int rc;
  if (p->kind() == AST::MatchN) {
    auto m = (Match *)p;
    StringRef pattern = evalPattern(m->getPattern());
    string target = eval(m->getTarget());
    rc = regEx->match(pattern, target);
    matchColumns = false;
  } else {
    StringRef pattern = evalPattern(p);
    rc = regEx->match(pattern, getCurrentLine());
    matchColumns = false;
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
  if (debug) {
    std::cout << "trace " << inputBuffer->getLineno() << ":";
    stmt->dumpOne();
  }
  switch (stmt->kind()) {
  case AST::SkipN:
    return NEXT_S;
  case AST::CopyN:
    outputBuffer->append(getCurrentLine());
    return NEXT_S;
  case AST::PrintN: {
    auto p = (Print *)stmt;
    LineBuffer *out = outputBuffer;
    string value = eval(p->getText());
    if (auto b = p->getBuffer()) {
      out = LineBuffer::findOutputBuffer(eval(b));
    }
    out->append(value);
    break;
  }
  case AST::ReplaceN: {
    auto r = (Replace *)stmt;
    auto pattern = evalPattern(r->getPattern());
    auto index = regEx->setPattern(pattern);
    if (index < 0)
      return STOP_S;

    auto target = eval(r->getReplacement());
    currentLine_ = regEx->replace(index, target, getCurrentLine());
    regEx->releasePattern(index);
    break;
  }
  case AST::ForeachN:
    return interpret((Foreach *)stmt);
  case AST::IfStmtN:
    return interpret((IfStatement *)stmt);
  case AST::SetN:
    return interpret((Set *)stmt);
  case AST::ColumnsN:
    return interpret((Columns *)stmt);
  case AST::ErrorN: {
    auto e = (Error *)stmt;
    auto msg = eval(e->getText());
    error() << msg << '\n';
    return NEXT_S;
  }
  case AST::InputN:
  case AST::OutputN:
    assert(!"not yet implemented");
    return STOP_S;
  }
  return OK_S;
}

ResultCode State::interpret(Set *set) {
  auto rhs = eval(set->getRhs());
  set->getSymbol().setValue(rhs);
  return OK_S;
}

static bool getUnsigned(const string &str, unsigned *u) {
  stringstream ss(str);
  ss >> *u;
  return !ss.fail();
}

ResultCode State::interpret(Columns *cols) {
  matchColumns = true;
  columns.clear();
  vector<unsigned> nums;

  auto process = [this, &nums](const string &text) {
    unsigned u;
    if (getUnsigned(text, &u)) {
      nums.push_back(u);
    } else {
      error() << "number expected for column: " << text << '\n';
    }
  };

  cols->getColumns()->walkDown([&process, &nums, this](Expression *e) {
    switch (e->kind()) {
    case AST::StringConstN:
      process(((StringConst *)e)->getConstant().getText());
      break;
    case AST::VariableN:
      process(((Variable *)e)->getSymbol().getValue());
      break;
    case AST::IntegerN: {
      auto u = ((Integer *)e)->getValue();
      if (u < 0) {
        error() << "negative value invalid as column: " << u << '\n';
      } else {
        nums.push_back(u);
      }
      break;
    }
    case AST::CallN:
      process(evalCall((Call *)e));
      break;
    case AST::BinaryN:
      assert(BinaryP(e)->op == Binary::CONCAT);
      break;
    case AST::ArgN:
    case AST::BufferN:
    case AST::ControlN:
    case AST::NotN:
    case AST::MatchN:
      assert(!"invalid expresion in pattern");
    }
    return AST::ContinueW;

  });
  if (sawError)
    return STOP_S;

  int last = -1;
  for (auto c : nums) {
    if (int(c) < last) {
      error() << "column values must be non-decreasing " << c;
      return STOP_S;
    }
    last = c;
  }

  unsigned lastC = 0;
  string inExpr = (cols->getInExpr() ? eval(cols->getInExpr()) : string(""));
  auto &current = (cols->getInExpr() ? inExpr : getCurrentLine());
  for (auto c : nums) {
    if (c > current.length()) {
      c = (unsigned)current.length();
    }
    columns.push_back(current.substr(lastC, (c - lastC)));
    // std::cout << (columns.size() -1) << " " << columns.back() << '\n';
    lastC = c;
  }
  if (last < current.length()) {
    columns.push_back(current.substr(last));
  } else {
    columns.push_back("");
  }
  return OK_S;
}

string State::evalCall(Call *c) {
  auto a = c->getArgs();
  vector<StringRef> args;
  while (a) {
    assert(a->kind() == a->ArgN);
    args.emplace_back(evalPattern(((Arg *)a)->getValue()));
    a = a->getNext();
  }
  return BuiltinCalls::evalCall(c->getCallId(), args, this);
}

void Interpreter::initialize() {
  state = new State;
  state->inputBuffer = makeInBuffer(&std::cin);
  state->outputBuffer = makeOutBuffer(&std::cout);
  state->setRegEx(RegEx::regEx);
  Symbol::defineSymbol(makeSymbol("LINE", [this]() {
    auto line = state->inputBuffer->getLineno();
    stringstream buf;
    buf << line;
    string result = buf.str();
    return result;
  }));
  Symbol::defineSymbol(
      makeSymbol("CURRENT", [this]() { return state->getCurrentLine(); }));

  Symbol::defineSymbol(makeSymbol("SOURCE", [this]() {
    state->getInputEof();
    return state->inputLine;
  }));
  for (unsigned i = 0; i < 10; i++) {
    stringstream nameBuffer;
    nameBuffer << i;
    Symbol::defineSymbol(
        makeSymbol(nameBuffer.str(), [this, i]() { return state->match(i); }));
  }
}

bool Interpreter::setInput(const string &fileName) {
  auto f = new std::ifstream(fileName);
  if (!f || !f->is_open()) {
    std::cerr << "unable to open: " << fileName << '\n';
    return false;
  }
  state->inputBuffer = makeInBuffer(f);
  return true;
}

void Interpreter::interpret(Statement *script) { state->interpret(script); }

int Interpreter::getReturnCode() const { return state ? state->sawError : 0; }
