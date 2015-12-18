//
//  Interpreter.cpp
//  rsed
//
//  Created by David Callahan on 6/27/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include "Interpreter.h"
#include <assert.h>
#include <sstream>
#include <iostream>
#include <vector>
#include "Symbol.h"
#include "AST.h"
#include "ASTWalk.h"
#include "LineBuffer.h"
#include "RegEx.h"
#include "BuiltinCalls.h"
#include "EvalState.h"
#include "Exception.h"

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
  LineBuffer *inputBuffer;

public:
  bool firstLine = true;

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
  // todo, move input state to line buffer...
  LineBuffer *getInputBuffer() const { return inputBuffer; }

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
  void resetInput(LineBuffer *newBuffer) {
    firstLine = true;
    currentLine_ = "";
    inputLine = "";
    inputEof_ = false;
    inputBuffer = newBuffer;
  }

  LineBuffer *outputBuffer;

  ResultCode interpret(Statement *stmtList);
  ResultCode interpretOne(Statement *stmt);
  ResultCode interpret(Foreach *foreach);
  ResultCode interpret(IfStatement *ifstmt);
  ResultCode interpret(Set *set);
  ResultCode interpret(Columns *cols);
  ResultCode interpret(Split *split);
  bool interprettPredicate(Expression *predicate);
  bool interpretMatch(const StringRef &pattern, const string &target);

  string eval(Expression *ast);
  string expandVariables(const string &text) override;
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
  if (auto p = c->pattern) {
    if (p->isOp(Expression::NOT)) {
      negate = true;
      p = BinaryP(p)->left;
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
      const auto &text = sc.getText();
      if (sc.isRaw()) {
        s << text;
      } else {
        s << expandVariables(text);
      }
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
      return AST::SkipChildrenW;
    case AST::BinaryN: {
      auto b = BinaryP(e);
      switch (b->op) {
      case Expression::CONCAT:
        break;
      case Expression::LOOKUP: {
        auto v = eval(b->right);
        auto sym = Symbol::findSymbol(v);
        s << sym->getValue();
        return AST::SkipChildrenW;
      }
      default:
        assert("unimplemented binary operator");
        break;
      }
      break;
    }
    case AST::ControlN:
    case AST::ArgN:
      assert(!"invalid expresion in pattern");
    }
    return AST::ContinueW;
  });
  return StringRef(s.str(), flags);
}

static std::regex variable(R"((\\\$)|(\$[0-9a-zA-Z_]+))");

string State::expandVariables(const string &text) {

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
        if (c && c->getReuired()) {
          error() << foreach->getSourceLine()
                  << ": end of input reached before required pattern seen: "
                  << eval(c->pattern) << '\n';
          return STOP_S;
        }
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

  if (interprettPredicate(ifstmt->predicate)) { // true
    if (sawError)
      return STOP_S;
    return interpret(ifstmt->thenStmts);
  }
  if (auto e = ifstmt->elseStmts) {
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
    string value = eval(p->text);
    if (auto b = p->buffer) {
      out = LineBuffer::findOutputBuffer(eval(b));
    }
    out->append(value);
    break;
  }
  case AST::ReplaceN: {
    auto r = (Replace *)stmt;
    auto pattern = evalPattern(r->pattern);
    if (r->optAll) {
      pattern.setIsGlobal();
    }
    auto index = regEx->setPattern(pattern);
    if (index < 0)
      return STOP_S;

    auto target = eval(r->replacement);
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
  case AST::SplitN:
    return interpret((Split *)stmt);
  case AST::ErrorN: {
    auto e = (Error *)stmt;
    auto msg = eval(e->text);
    error() << msg << '\n';
    return NEXT_S;
  }
  case AST::InputN: {
    auto io = (Input *)stmt;
    auto fileName = eval(io->buffer);
    inputBuffer = LineBuffer::findInputBuffer(fileName);
    break;
  }
  case AST::OutputN: {
    auto io = (Output *)stmt;
    auto fileName = eval(io->buffer);
    outputBuffer = LineBuffer::findInputBuffer(fileName);
    break;
  }
  case AST::RewindN: {
    auto io = (Input *)stmt;
    auto fileName = eval(io->buffer);
    resetInput(LineBuffer::findInputBuffer(fileName));
    if (!inputBuffer->rewind()) {
      throw Exception("unable to rewrind input file: " + fileName, stmt,
                      getInputBuffer());
    }
    break;
  }
  case AST::RequiredN: {
    auto r = (Required *)stmt;
    if (r->predicate) {
      auto pred = r->predicate;
      if (interprettPredicate(pred)) {
        return OK_S;
      }
      const char *msg = ": failed required pattern: ";
      if (auto b = pred->isOp(Expression::NOT)) {
        msg = ": failed forbidde pattern: ";
        pred = b->right;
      }
      error() << " source " << stmt->getSourceLine() << msg << eval(pred)
              << '\n';
    } else {
      if (matchColumns && columns.size() >= r->getCount()) {
        return OK_S;
      }
      error() << "failed required column count: " << r->getCount() << '\n';
    }
    return STOP_S;
  }
  }
  return OK_S;
}

ResultCode State::interpret(Set *set) {
  auto rhs = eval(set->rhs);
  auto lhs = set->lhs;
  if (lhs->kind() == AST::VariableN) {
    ((Variable *)lhs)->getSymbol().setValue(rhs);
  } else if (lhs->isOp(lhs->LOOKUP)) {
    auto b = BinaryP(lhs);
    auto name = eval(b->right);
    auto symbol = Symbol::findSymbol(name);
    symbol->setValue(rhs);
  } else {
    assert("not yet implemented non variable lhs");
  }
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

  cols->columns->walkDown([&process, &nums, this](Expression *e) {
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
    case AST::ControlN:
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
  string inExpr = (cols->inExpr ? eval(cols->inExpr) : string(""));
  auto &current = (cols->inExpr ? inExpr : getCurrentLine());
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

ResultCode State::interpret(Split *split) {
  auto sep = evalPattern(split->separator);
  const string &target =
      (split->target ? eval(split->target) : getCurrentLine());
  matchColumns = true;
  columns.clear();
  auto ok = regEx->split(sep, target, &columns);
  return (ok ? OK_S : STOP_S);
}

string State::evalCall(Call *c) {
  vector<StringRef> args;
  for (auto a = c->args; a; a = a->nextArg) {
    args.emplace_back(evalPattern(a->value));
  }
  return BuiltinCalls::evalCall(c->getCallId(), args, this);
}

void Interpreter::initialize() {
  state = new State;
  state->resetInput(makeInBuffer(&std::cin, "<stdin>"));
  state->outputBuffer = makeOutBuffer(&std::cout, "<stdout>");
  state->setRegEx(RegEx::regEx);
  Symbol::defineSymbol(makeSymbol("LINE", [this]() {
    auto line = state->getLineno();
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
  state->resetInput(makeInBuffer(f, fileName));
  return true;
}

bool State::interprettPredicate(Expression *predicate) {
  if (auto b = predicate->isOp(Expression::NOT)) {
    return !interprettPredicate(b->right);
  }
  if (auto m = predicate->isOp(Expression::MATCH)) {
    return interpretMatch(evalPattern(m->right), eval(m->left));
  }
  return interpretMatch(evalPattern(predicate), getCurrentLine());
}
bool State::interpretMatch(const StringRef &pattern, const string &target) {
  matchColumns = false;
  auto rc = regEx->match(pattern, target);
  if (rc < 0) {
    error() << "invalid regular expression: " << pattern.getFlags() << '\n';
    return false;
  }
  return (rc > 0);
}

void Interpreter::interpret(Statement *script) { state->interpret(script); }

int Interpreter::getReturnCode() const { return state ? state->sawError : 0; }
