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
#include "Value.h"

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
  Value *interpret(Expression *);

  string expandVariables(const string &text) override;
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
    auto v = state->interpret(p);
    if (!v->isString()) {
      // TODO -- support more general preducates
      throw Exception("unsupported expression in foreach");
    }
    regIndex = state->getRegEx()->setPattern(v->asString());
  }
  return true;
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
    try {
      rc = interpretOne(stmt);
    }
    catch (Exception & e) {
      e.setStatement(stmt,inputBuffer);
      throw;
    }
    return (rc == OK_S ? AST::SkipChildrenW : AST::StopW);
  });
  return rc;
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
          // TODO add test case
          throw Exception("end of input reached before required pattern seen",
                          foreach, inputBuffer);
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
    string value = interpret(p->text)->asString().getText();
    if (auto b = p->buffer) {
      auto v = interpret(b);
      if (!v->isString()) {
        throw Exception("invalid file name" + v->asString(), stmt, inputBuffer);
      }
      out = LineBuffer::findOutputBuffer(v->asString().getText());
    }
    out->append(value);
    break;
  }
  case AST::ReplaceN: {
    auto r = (Replace *)stmt;
    auto pattern = interpret(r->pattern)->asString();
    if (r->optAll) {
      pattern.setIsGlobal();
    }
    auto index = regEx->setPattern(pattern);
    if (index < 0)
      return STOP_S;

    auto target = interpret(r->replacement)->asString().getText();
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
    // TODO add test cases
    auto e = (Error *)stmt;
    auto msg = interpret(e->text)->asString().getText();
    throw Exception(msg, stmt, inputBuffer);
  }
  case AST::InputN: {
    auto io = (Input *)stmt;
    auto fileName = interpret(io->buffer)->asString().getText();
    inputBuffer = LineBuffer::findInputBuffer(fileName);
    break;
  }
  case AST::OutputN: {
    auto io = (Output *)stmt;
    auto fileName = interpret(io->buffer)->asString().getText();
    outputBuffer = LineBuffer::findInputBuffer(fileName);
    break;
  }
  case AST::RewindN: {
    auto io = (Input *)stmt;
    auto fileName = interpret(io->buffer)->asString().getText();
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
      const char *msg = "failed required pattern: ";
      if (auto b = pred->isOp(Expression::NOT)) {
        msg = "failed forbidden pattern: ";
        pred = b->right;
      }
      throw Exception(msg, stmt, inputBuffer);
    } else {
      if (matchColumns && columns.size() >= r->getCount()) {
        return OK_S;
      }
      throw Exception("failed required column count", stmt, inputBuffer);
    }
    return STOP_S;
  }
  }
  return OK_S;
}

ResultCode State::interpret(Set *set) {
  auto rhs = interpret(set->rhs)->asString().getText();
  auto lhs = set->lhs;
  if (lhs->kind() == AST::VariableN) {
    ((Variable *)lhs)->getSymbol().setValue(rhs);
  } else if (lhs->isOp(lhs->LOOKUP)) {
    auto b = BinaryP(lhs);
    auto name = interpret(b->right);
    if (!name->isString()) {
      throw Exception("invalid symbol name " + name->asString(), set,
                      inputBuffer);
    }
    auto symbol = Symbol::findSymbol(name->asString().getText());
    symbol->setValue(rhs);
  } else {
    assert("not yet implemented non variable lhs");
  }
  return OK_S;
}

ResultCode State::interpret(Columns *cols) {
  matchColumns = true;
  columns.clear();
  vector<unsigned> nums;

  string inExpr =
      (cols->inExpr ? interpret(cols->inExpr)->asString().getText() : "");
  auto &current = (cols->inExpr ? inExpr : getCurrentLine());

  int lastC = 0;
  int max = current.length();
  auto addCol = [this, &lastC, max, cols, current](Expression * e) {
    auto v = interpret(e);
    auto i = int(v->asNumber());
    if (i < lastC) {
      throw Exception("column numbers must be positive and non-decreasing: " +
                      v->asString(),
                      cols, inputBuffer);
    }
    i = std::min(i, max);
    columns.push_back(current.substr(lastC, (i - lastC)));
    lastC = i;
  };
  
  auto c = cols->columns;
  for ( ; c->isOp(c->CONCAT); c = BinaryP(c)->right) {
    addCol(BinaryP(c)->left);
  }
  addCol(c);

  if (lastC < current.length()) {
    columns.push_back(current.substr(lastC));
  } else {
    columns.push_back("");
  }
  return OK_S;
}

ResultCode State::interpret(Split *split) {
  auto sep = interpret(split->separator)->asString().getText();
  const string &target =
      (split->target ? interpret(split->target)->asString().getText()
                     : getCurrentLine());
  matchColumns = true;
  columns.clear();
  auto ok = regEx->split(sep, target, &columns);
  return (ok ? OK_S : STOP_S);
}

string State::evalCall(Call *c) {
  vector<StringRef> args;
  for (auto a = c->args; a; a = a->nextArg) {
    args.emplace_back(interpret(a->value)->asString());
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
  auto v = interpret(predicate);
  if (v->isString()) {
    return interpretMatch(v->asString(), getCurrentLine());
  }
  return v->asLogical();
}
bool State::interpretMatch(const StringRef &pattern, const string &target) {
  matchColumns = false;
  auto rc = regEx->match(pattern, target);
  if (rc < 0) {
    throw Exception("invalid regular expression: " + pattern.getText());
  }
  return (rc > 0);
}

void Interpreter::interpret(Statement *script) { state->interpret(script); }

Value *State::interpret(Expression *e) {
  switch (e->kind()) {
  case AST::ArgN:
    assert("unexpected arg");
    break;
  case AST::ControlN:
    assert("internal error: unexpected control");
    break;
  case AST::VariableN: {
    e->set(((Variable *)e)->getSymbol().getValue());
    break;
  }
  case AST::IntegerN: {
    e->set(double(((Integer *)e)->getValue()));
    break;
  }
  case AST::StringConstN: {
    const auto &sc = ((StringConst *)e)->getConstant();
    if (sc.isRaw()) {
      e->set(sc);
    } else {
      e->set(expandVariables(sc.getText()));
    }
    break;
  }
  case AST::CallN: {
    auto c = (Call *)e;
    vector<StringRef> args;
    for (auto a = c->args; a; a = a->nextArg) {
      args.emplace_back(interpret(a->value)->asString());
    }
    e->set(BuiltinCalls::evalCall(c->getCallId(), args, this));
    break;
  }
  case AST::BinaryN: {
    auto b = (Binary *)e;
    switch (b->op) {
    case Binary::NOT: {
      auto v = interpret(b->right);
      if (v->isString()) {
        e->set(!interpretMatch(v->asString(), getCurrentLine()));
      } else {
        e->set(!v->asLogical());
      }
      break;
    }
    case Binary::LOOKUP: {
      auto sym = Symbol::findSymbol(interpret(b->right)->asString().getText());
      e->set(sym->getValue());
      break;
    }
    case Binary::CONCAT: {
      auto str = interpret(b->left)->asString();
      str.append(interpret(b->right)->asString());
      e->set(str);
      break;
    }
    case Binary::MATCH: {
      auto pattern = interpret(b->right)->asString();
      auto target = interpret(b->left)->asString().getText();
      e->set(interpretMatch(pattern, target));
      break;
    }
    case Binary::REPLACE:
    case Binary::ADD:
    case Binary::SUB:
    case Binary::MUL:
    case Binary::DIV:
    case Binary::NEG:
    case Binary::LT:
    case Binary::LE:
    case Binary::EQ:
    case Binary::NE:
    case Binary::GE:
    case Binary::GT:
    case Binary::AND:
    case Binary::OR:
      assert("binary operator not yet implemented");
      break;
    }
  } break;
  }
  return e;
}
