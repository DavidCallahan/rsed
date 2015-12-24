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

namespace {
enum ResultCode {
  OK_S,   // continue
  NEXT_S, // skip to next line
};
enum MatchKind { NoMatchK, StopAtK, StopAfterK };
}

class State : public EvalState {
  LineBuffer *inputBuffer;

public:
  // use columns are match for $1, $2,...
  bool matchColumns = true;
  vector<string> columns;

  string currentLine_;
  bool firstLine = true;
  string &getCurrentLine() {
    if (firstLine) {
      nextLine();
    }
    return currentLine_;
  }

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
  const string &getInputLine() {
    if (firstLine) {
      nextLine();
    }
    return inputBuffer->getInputLine();
  }
  void nextLine() {
    if (!inputEof_) {
      inputEof_ = !inputBuffer->getLine(currentLine_);
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
    inputEof_ = false;
    inputBuffer = newBuffer;
  }

  LineBuffer *outputBuffer;

  ResultCode interpret(Statement *stmtList);
  ResultCode interpretOne(Statement *stmt);
  void interpret(Foreach *foreach);
  ResultCode interpret(IfStatement *ifstmt);
  void interpret(Set *set);
  void interpret(Columns *cols);
  void interpret(Split *split);

  bool interprettPredicate(Expression *predicate);
  bool interpretMatch(const StringRef &pattern, const string &target);
  Value *interpret(Expression *);
  void interpret(Expression *, stringstream &, unsigned *flags);
  void print(Expression *, LineBuffer *);

  // string expandVariables(const string &text) override;
  stringstream &expandVariables(const string &text, stringstream &str) override;
  // evaluate match control against current linke
  MatchKind matchPattern(AST &);
  bool isMatch(Expression *);
};

class ForeachControl {
  State *state;
  int regIndex;
  bool negate;
  MatchKind matchKind;
  bool hasCount;
  unsigned count;
  bool all;
  Expression *predicate = nullptr;

public:
  ForeachControl(State *state)
      : state(state), regIndex(-1), negate(false), matchKind(NoMatchK),
        hasCount(true), count(0), all(false) {}
  void initialize(Control *c);
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
  if (predicate) {
    if (state->interprettPredicate(predicate)) {
      return matchKind;
    }
    return NoMatchK;
  }
  return NoMatchK;
}

void ForeachControl::initialize(Control *c) {
  if (!c) {
    all = true;
    return;
  }
  hasCount = c->hasLimit();
  if (hasCount) {
    count = (unsigned)c->getLimit();
  }
  matchKind = (c->getStopKind() == AST::StopAt ? StopAtK : StopAfterK);
  regIndex = -1;
  predicate = nullptr;
  if (auto p = c->pattern) {
    if (state->isMatch(p)) {
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
    } else {
      predicate = p;
    }
  }
}

static std::regex variable(R"((\\\$)|(\$[a-zA-Z][a-zA-Z0-9_]+)|(\$[0-9]+))");

stringstream &State::expandVariables(const string &text, stringstream &str) {

  const char *ctext = text.c_str();
  size_t len = text.length();
  auto vars_begin = std::cregex_iterator(ctext, ctext + len, variable);
  auto vars_end = std::cregex_iterator();
  if (vars_begin == vars_end) {
    str << text;
    return str;
  }

  const char *last = ctext;
  for (std::cregex_iterator vars = vars_begin; vars != vars_end; ++vars) {
    auto match = (*vars)[0];
    const char *cvar = match.first;
    str.write(last, cvar - last);

    if (*cvar == '\\') {
      str << '$';
    } else if (isdigit(cvar[1])) {
      stringstream temp;
      temp.write(cvar + 1, match.second - cvar - 1);
      unsigned i;
      temp >> i;
      str << State::match(i);
    } else {
      str << Symbol::findSymbol(string(cvar + 1, match.second - cvar - 1))
                 ->getValue();
    }
    last = match.second;
  }
  str.write(last, len - (last - ctext));
  return str;
}

/// interpret all statements on a list until an error
/// is seen or som
ResultCode State::interpret(Statement *stmtList) {
  ResultCode rc = OK_S;
  stmtList->walk([this, &rc](Statement *stmt) {
    try {
      rc = interpretOne(stmt);
    } catch (Exception &e) {
      e.setStatement(stmt, inputBuffer);
      throw;
    }
    return (rc == OK_S ? AST::SkipChildrenW : AST::StopW);
  });
  return rc;
}

void State::interpret(Foreach *foreach) {
  auto c = (Control *)foreach->control;
  ForeachControl fc(this);
  fc.initialize(c);

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
    interpret(b);
    nextLine();
    if (mk == StopAfterK) {
      break;
    }
  }
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
    auto target = interpret(r->replacement)->asString().getText();
    currentLine_ = regEx->replace(index, target, getCurrentLine());
    regEx->releasePattern(index);
    break;
  }
  case AST::ForeachN:
    interpret((Foreach *)stmt);
    break;
  case AST::IfStmtN:
    return interpret((IfStatement *)stmt);
  case AST::SetN:
    interpret((Set *)stmt);
    break;
  case AST::ColumnsN:
    interpret((Columns *)stmt);
    break;
  case AST::SplitN:
    interpret((Split *)stmt);
    break;
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
  }
  }
  return OK_S;
}

void State::print(Expression *e, LineBuffer *out) {
  while (auto b = e->isOp(e->CONCAT)) {
    print(b->left, out);
    e = b->right;
  }
  auto v = interpret(e);
  out->append(v->asString().getText());
}

void State::interpret(Set *set) {
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
}

void State::interpret(Columns *cols) {
  matchColumns = true;
  columns.clear();
  vector<unsigned> nums;

  string inExpr =
      (cols->inExpr ? interpret(cols->inExpr)->asString().getText() : "");
  auto &current = (cols->inExpr ? inExpr : getCurrentLine());

  int lastC = 0;
  int max = current.length();
  auto addCol = [this, &lastC, max, cols, current](Expression *e) {
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
  for (; c->isOp(c->CONCAT); c = BinaryP(c)->right) {
    addCol(BinaryP(c)->left);
  }
  addCol(c);

  if (lastC < current.length()) {
    columns.push_back(current.substr(lastC));
  } else {
    columns.push_back("");
  }
}

void State::interpret(Split *split) {
  auto sep = interpret(split->separator)->asString().getText();
  const string &target =
      (split->target ? interpret(split->target)->asString().getText()
                     : getCurrentLine());
  matchColumns = true;
  columns.clear();
  regEx->split(sep, target, &columns);
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
    return state->getInputLine();
  }));
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
  return regEx->match(pattern, target);
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
  case AST::VarMatchN: {
    e->set(match(((VarMatch *)e)->getValue()));
    break;
  }
  case AST::StringConstN: {
    const auto &sc = ((StringConst *)e)->getConstant();
    if (sc.isRaw()) {
      e->set(sc);
    } else {
      stringstream str;
      e->set(
          StringRef(expandVariables(sc.getText(), str).str(), sc.getFlags()));
    }
    break;
  }
  case AST::CallN: {
    auto c = (Call *)e;
    vector<Value *> args;
    for (auto a = c->args; a; a = a->nextArg) {
      args.emplace_back(interpret(a->value));
    }
    BuiltinCalls::evalCall(c->getCallId(), args, this, e);
    break;
  }
  case AST::RegExPatternN:
  case AST::RegExReferenceN:
    assert(0 && "not yet implemented");
    break;
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
      stringstream str;
      unsigned flags = 0;
      interpret(e, str, &flags);
      e->set(StringRef(str.str(), flags));
      break;
    }
    case Binary::MATCH: {
      auto pattern = interpret(b->right)->asString();
      auto target = interpret(b->left)->asString().getText();
      e->set(interpretMatch(pattern, target));
      break;
    }
    case Binary::REPLACE_ALL:
    case Binary::REPLACE: {
      auto m = (Binary *)b->left;
      assert(m->isOp(m->MATCH));
      auto pattern = interpret(m->right)->asString();
      if (e->isOp(e->REPLACE_ALL)) {
        pattern.setIsGlobal();
      }
      auto index = regEx->setPattern(pattern);
      auto input = interpret(m->left)->asString().getText();
      auto replacement = interpret(b->right)->asString().getText();
      b->set(regEx->replace(index, replacement, input));
      regEx->releasePattern(index);
      break;
    }
    case Binary::EQ:
      b->set(compare(interpret(b->left), interpret(b->right)) == 0);
      break;
    case Binary::NE:
      b->set(compare(interpret(b->left), interpret(b->right)) != 0);
      break;
    case Binary::LT:
      b->set(compare(interpret(b->left), interpret(b->right)) < 0);
      break;
    case Binary::LE:
      b->set(compare(interpret(b->left), interpret(b->right)) <= 0);
      break;
    case Binary::GE:
      b->set(compare(interpret(b->left), interpret(b->right)) >= 0);
      break;
    case Binary::GT:
      b->set(compare(interpret(b->left), interpret(b->right)) > 0);
      break;
    case Binary::ADD:
      b->set(interpret(b->left)->asNumber() + interpret(b->right)->asNumber());
      break;
    case Binary::SUB:
      b->set(interpret(b->left)->asNumber() - interpret(b->right)->asNumber());
      break;
    case Binary::MUL:
      b->set(interpret(b->left)->asNumber() * interpret(b->right)->asNumber());
      break;
    case Binary::DIV:
      b->set(interpret(b->left)->asNumber() / interpret(b->right)->asNumber());
      break;
    case Binary::NEG:
      b->set(-interpret(b->right)->asNumber());
      break;
    case Binary::AND:
      b->set(interpret(b->left)->asLogical() &&
             interpret(b->right)->asLogical());
      break;
    case Binary::OR:
      b->set(interpret(b->left)->asLogical() ||
             interpret(b->right)->asLogical());
      break;
    }
  } break;
  }
  return e;
}

void State::interpret(Expression *e, stringstream &str, unsigned *flags) {
  while (auto b = e->isOp(e->CONCAT)) {
    interpret(b->left, str, flags);
    e = b->right;
  }
  switch (e->kind()) {
  case AST::VariableN: {
    str << ((Variable *)e)->getSymbol().getValue();
    break;
  }
  case AST::IntegerN: {
    str << ((Integer *)e)->getValue();
    break;
  }
  case AST::StringConstN: {
    const auto &sc = ((StringConst *)e)->getConstant();
    if (sc.isRaw()) {
      str << sc.getText();
    } else {
      expandVariables(sc.getText(), str);
    }
    *flags |= sc.getFlags();
    break;
  }
  default: {
    auto v = interpret(e);
    str << v->asString().getText();
    *flags |= v->asString().getFlags();
    break;
  }
  }
}

bool State::isMatch(Expression *e) {
  if (e->kind() != e->BinaryN) {
    return true;
  }
  auto b = (Binary *)e;
  switch (b->op) {
  case Binary::EQ:
  case Binary::NE:
  case Binary::LT:
  case Binary::LE:
  case Binary::GE:
  case Binary::GT:
  case Binary::AND:
  case Binary::OR:
    return false;
  case Binary::NOT:
    return isMatch(b->right);
  default:
    return true;
  }
}
