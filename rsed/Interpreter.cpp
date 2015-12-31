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
#include "rsed.h"
#include "Symbol.h"
#include "AST.h"
#include "ASTWalk.h"
#include "LineBuffer.h"
#include "RegEx.h"
#include "BuiltinCalls.h"
#include "EvalState.h"
#include "Exception.h"
#include "Value.h"
#include "ExpandVariables.h"

using std::vector;
using std::string;
using std::stringstream;

namespace RSED_Debug {
extern int debug;
}

namespace {
enum ResultCode {
  OK_S,   // continue
  NEXT_S, // skip to next line
};
enum MatchKind { NoMatchK, StopAtK, StopAfterK };
}

class State : public EvalState {
  std::shared_ptr<LineBuffer> inputBuffer;

  vector<std::shared_ptr<LineBuffer>> inputStack;
  vector<std::shared_ptr<LineBuffer>> outputStack;

public:
  std::shared_ptr<LineBuffer> stdinBuffer;
  std::shared_ptr<LineBuffer> stdoutBuffer;
  // use columns are match for $1, $2,...
  bool matchColumns = true;
  vector<string> columns;

  string currentLine_;
  bool needLine = true;
  string &getCurrentLine() {
    if (needLine) {
      nextLine();
    }
    return currentLine_;
  }

  std::shared_ptr<LineBuffer> getInputBuffer() const { return inputBuffer; }
  void pushInput(std::shared_ptr<LineBuffer> newInput) {
    inputStack.push_back(std::move(inputBuffer));
    resetInput(newInput);
  }
  void popInput() {
    if (!inputStack.empty()) {
      resetInput(inputStack.back());
      inputStack.pop_back();
    }
  }
  void pushOutput(std::shared_ptr<LineBuffer> newOutput) {
    outputStack.push_back(std::move(outputBuffer));
    outputBuffer = std::move(newOutput);
  }
  void popOutput() {
    if (!outputStack.empty()) {
      outputBuffer = outputStack.back();
      outputStack.pop_back();
    }
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
  bool inputEof_ = false;
  bool getInputEof() {
    if (needLine) {
      nextLine();
    }
    return inputEof_;
  }
  const string &getInputLine() {
    if (needLine) {
      nextLine();
    }
    return inputBuffer->getInputLine();
  }
  void nextLine() {
    if (!inputEof_) {
      inputEof_ = !inputBuffer->getLine(currentLine_);
      if (RSED_Debug::debug) {
        std::cout << "input: " << currentLine_ << "\n";
      }
      needLine = false;
    }
  }
  unsigned getLineno() const override {
    return inputBuffer->getLineno() + unsigned(needLine);
  }
  void resetInput(const std::shared_ptr<LineBuffer> &newBuffer) {
    needLine = true;
    currentLine_ = "";
    inputEof_ = false;
    inputBuffer = newBuffer;
  }

  std::shared_ptr<LineBuffer> outputBuffer;

  ResultCode interpret(Statement *stmtList);
  ResultCode interpretOne(Statement *stmt);
  void interpret(Foreach *foreach);
  ResultCode interpret(IfStatement *ifstmt);
  void interpret(Set *set);
  void interpret(Columns *cols, vector<string> *columns);
  void interpret(Split *split);

  bool interprettPredicate(Expression *predicate);
  bool interpretMatch(int pattern, const string &target);
  Value *interpret(Expression *);
  void print(Expression *, LineBuffer *);
  void print(Value *, LineBuffer *);
  Value *getPattern(Expression *);
  string getRequiredMessage(Expression *pattern, Expression *errMsg);

  // string expandVariables(const string &text) override;
  stringstream &expandVariables(const string &text, stringstream &str) override;
};

class ForeachControl {
  State *state;
  MatchKind matchKind = NoMatchK;
  bool hasCount = true;
  unsigned count = 0;
  bool all = false;
  Expression *predicate = nullptr;

public:
  ForeachControl(State *state) : state(state) {}
  void initialize(Control *c);
  MatchKind eval(const string *line);
  void release() {}
  bool needsInput() const { return all || predicate; }
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
    if (count == 0)
      return StopAfterK;
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
  predicate = nullptr;
  if (!c) {
    all = true;
    return;
  }
  hasCount = c->hasLimit();
  if (hasCount) {
    count = (unsigned)c->getLimit();
  }
  matchKind = (c->getStopKind() == AST::StopAt ? StopAtK : StopAfterK);
  predicate = c->pattern;
}

namespace {
class DynamicExpander : public ExpandVariables {
  State &state;
  stringstream &out;

public:
  DynamicExpander(State &state, stringstream &out) : state(state), out(out) {}
  void single(const std::string &text, unsigned) override { out << text; }
  void string(stringstream &s, unsigned) override { out << s.str(); }
  void varMatch(unsigned i) override { out << state.match(i); };
  void variable(std::string name) override {
    out << Symbol::findSymbol(name)->getValue()->asString().getText();
  }
};
}

stringstream &State::expandVariables(const string &text, stringstream &str) {
  DynamicExpander expander(*this, str);
  expander.expand(text);
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
        if (c && c->getRequired()) {
          string msg = getRequiredMessage(c->pattern, c->errorMsg);
          throw Exception("at end of input, " + msg, foreach, inputBuffer);
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
    if (mk == StopAfterK) {
      needLine = true;
      break;
    }
    nextLine();
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
  if (RSED_Debug::debug) {
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
    auto out = outputBuffer;
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
    auto reg = interpret(r->pattern)->getRegEx();
    auto target = interpret(r->replacement)->asString().getText();
    currentLine_ = regEx->replace(reg, target, getCurrentLine());
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
    matchColumns = true;
    interpret((Columns *)stmt, &columns);
    break;
  case AST::SplitN:
    interpret((Split *)stmt);
    break;
  case AST::ErrorN: {
    auto e = (Error *)stmt;
    auto msg = interpret(e->text)->asString().getText();
    throw Exception(msg, stmt, inputBuffer);
  }
  case AST::InputN: {
    auto io = (Input *)stmt;
    auto value = interpret(io->buffer);
    if (value->kind == Value::List) {
      vector<string> data;
      for (auto &v : value->list) {
        data.emplace_back(v.asString().getText());
      }
      pushInput(LineBuffer::makeVectorInBuffer(&data, "from list"));
    } else {
      auto fileName = value->asString().getText();
      extern std::ofstream env_save;
      static unsigned count = 0;
      if (env_save.is_open()) {
        env_save << (io->getShellCmd() ? "#shell " : "#file ") << count++
                 << " \"" << fileName << "\"\n";  
      }
      if (io->getShellCmd()) {
        // todo: how does "close" work here?
        pushInput(LineBuffer::makePipeBuffer(fileName));
      } else {
        pushInput(LineBuffer::findInputBuffer(fileName));
      }
    }
    break;
  }
  case AST::SplitInputN: {
    auto split = (SplitInput *)stmt;
    auto sep = interpret(split->separator)->getRegEx();
    const string &target = interpret(split->target)->asString().getText();
    vector<string> data;
    regEx->split(sep, target, &data);
    pushInput(LineBuffer::makeVectorInBuffer(&data, target));
    break;
  }
  case AST::ColumnsInputN: {
    vector<string> data;
    interpret((Columns *)stmt, &data);
    pushInput(LineBuffer::makeVectorInBuffer(&data, "columns"));
    break;
  }
  case AST::OutputN: {
    auto io = (Output *)stmt;
    auto fileName = interpret(io->buffer)->asString().getText();
    outputBuffer = LineBuffer::findInputBuffer(fileName);
    break;
  }
  case AST::CloseN: {
    auto io = (Close *)stmt;
    switch (io->getMode()) {
    case Close::Input:
      inputBuffer->close();
      popInput();
      break;
    case Close::Output:
      outputBuffer->close();
      popOutput();
      break;
    case Close::ByName:
      auto fileName = interpret(io->buffer)->asString().getText();
      auto old = LineBuffer::closeBuffer(fileName);
      if (old == inputBuffer) {
        popInput();
      } else if (old == outputBuffer) {
        popOutput();
      }
      break;
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
      const char *msg = "failed required pattern";
      if (auto b = pred->isOp(Expression::NOT)) {
        msg = "failed forbidden pattern";
        pred = b->right;
      }
      string smsg(msg);
      if (auto pattern = getPattern(pred)) {
        smsg += ": \"" + pattern->asString().getText() + '"';
      }
      if (r->errMsg) {
        auto e = interpret(r->errMsg);
        smsg += ": " + e->asString().getText();
      }
      throw Exception(smsg, stmt, inputBuffer);
    } else {
      if (matchColumns && columns.size() >= r->getCount()) {
        return OK_S;
      }
      throw Exception("failed required column count", stmt, inputBuffer);
    }
  }
  case AST::HoistedValueN: {
    auto h = (HoistedValue *)stmt;
    interpret(h->rhs);
    break;
  }
  }
  return OK_S;
}

string State::getRequiredMessage(Expression *e, Expression *errMsg) {
  const char *msg = "failed required pattern";
  if (auto b = e->isOp(Expression::NOT)) {
    msg = "failed forbidden pattern";
    e = b->right;
  }
  string smsg(msg);
  if (auto pattern = getPattern(e)) {
    smsg += ": \"" + pattern->asString().getText() + '"';
  }
  if (errMsg) {
    auto e = interpret(errMsg);
    smsg += ": " + e->asString().getText();
  }
  return smsg;
}

Value *State::getPattern(Expression *e) {
  if (e->kind() == AST::RegExPatternN) {
    return ((RegExPattern *)e)->pattern;
  }
  if (e->kind() == AST::HoistedValueRefN) {
    return getPattern(((HoistedValueRef *)e)->value);
  }
  if (auto b = e->isOp(e->MATCH)) {
    return getPattern(b->right);
  }
  return nullptr;
}

void State::print(Expression *e, LineBuffer *out) {
  while (auto b = e->isOp(e->CONCAT)) {
    print(b->left, out);
    e = b->right;
  }
  print(interpret(e), out);
}

void State::print(Value *v, LineBuffer *out) {
  if (v->kind == v->List) {
    for (auto &lv : v->list) {
      print(&lv, out);
    }
  } else {
    out->append(v->asString().getText());
  }
}

void State::interpret(Set *set) {
  auto rhs = interpret(set->rhs);
  auto lhs = set->lhs;
  if (lhs->kind() == AST::VariableN) {
    ((Variable *)lhs)->getSymbol().set(rhs);
    if (RSED_Debug::debug) {
      std::cout << "set to " << *rhs << '\n';
    }
  } else if (lhs->isOp(lhs->LOOKUP)) {
    auto b = BinaryP(lhs);
    auto name = interpret(b->right);
    if (!name->isString()) {
      throw Exception("invalid symbol name " + name->asString(), set,
                      inputBuffer);
    }
    auto symbol = Symbol::findSymbol(name->asString().getText());
    symbol->set(rhs);
  } else {
    assert("not yet implemented non variable lhs");
  }
}

void State::interpret(Columns *cols, vector<string> *columns) {
  columns->clear();
  vector<unsigned> nums;

  string inExpr = interpret(cols->inExpr)->asString().getText();

  int lastC = 0;
  int max = inExpr.length();
  auto addCol = [this, &lastC, columns, max, cols, inExpr](Expression *e) {
    auto v = interpret(e);
    auto i = int(v->asNumber());
    if (i < lastC) {
      throw Exception("column numbers must be positive and non-decreasing: " +
                          v->asString(),
                      cols, inputBuffer);
    }
    i = std::min(i, max);
    columns->push_back(inExpr.substr(lastC, (i - lastC)));
    lastC = i;
  };

  cols->columns->walkConcat(addCol);

  if (lastC < inExpr.length()) {
    columns->push_back(inExpr.substr(lastC));
  } else {
    columns->push_back("");
  }
}

void State::interpret(Split *split) {
  auto sep = interpret(split->separator)->getRegEx();
  const string &target = interpret(split->target)->asString().getText();
  matchColumns = true;
  columns.clear();
  regEx->split(sep, target, &columns);
}

void Interpreter::initialize(int argc, char *argv[]) {
  state = new State;
  state->stdinBuffer = LineBuffer::makeInBuffer(&std::cin, "<stdin>");
  state->resetInput(state->stdinBuffer);
  state->stdoutBuffer = LineBuffer::makeOutBuffer(&std::cout, "<stdout>");
  state->outputBuffer = state->stdoutBuffer;
  state->setRegEx(RegEx::regEx);
  Symbol::defineSymbol(makeSymbol("LINE", [this]() {
    auto line = state->getLineno();
    stringstream buf;
    buf << line;
    string result = buf.str();
    return result;
  }));
  Symbol::defineSymbol(makeSymbol(
      AST::CURRENT_LINE_SYM, [this]() { return state->getCurrentLine(); }));

  Symbol::defineSymbol(makeSymbol("SOURCE", [this]() {
    state->getInputEof();
    return state->getInputLine();
  }));

  for (auto i = 1; i < argc; i++) {
    stringstream buf;
    buf << "ARG" << i;
    Symbol::defineSymbol(new SimpleSymbol(buf.str(), argv[i]));
  }
}

bool Interpreter::setInput(const string &fileName) {
  auto f = new std::ifstream(fileName);
  if (!f || !f->is_open()) {
    std::cerr << "unable to open: " << fileName << '\n';
    return false;
  }
  state->resetInput(LineBuffer::makeInBuffer(f, fileName));
  return true;
}

bool State::interprettPredicate(Expression *predicate) {
  auto v = interpret(predicate);
  assert(!v->isString());
  return v->asLogical();
}
bool State::interpretMatch(int pattern, const string &target) {
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
  case AST::NumberN: {
    e->set(((Number *)e)->getValue());
    break;
  }
  case AST::LogicalN: {
    e->set(((Logical *)e)->getValue());
    break;
  }
  case AST::VarMatchN: {
    e->set(match(((VarMatch *)e)->getValue()));
    break;
  }
  case AST::StringConstN: {
    e->set(((StringConst *)e)->getConstant());
    break;
  }
  case AST::CallN: {
    auto c = (Call *)e;
    vector<Value *> args;
    for (auto a = c->head; a; a = a->nextArg) {
      args.emplace_back(interpret(a->value));
    }
    BuiltinCalls::evalCall(c->getCallId(), args, this, e);
    break;
  }
  case AST::ListN: {
    auto l = ListP(e);
    l->clearList();
    for (auto a = l->head; a; a = a->nextArg) {
      l->append(*interpret(a->value));
    }
    break;
  }
  case AST::RegExPatternN: {
    auto r = (RegExPattern *)e;
    auto pattern = interpret(r->pattern)->asString();
    regEx->setPattern(pattern, r->getIndex());
    r->setRegEx(r->getIndex());
    break;
  }
  case AST::HoistedValueRefN: {
    auto h = (HoistedValueRef *)e;
    while (h->value->kind() == AST::HoistedValueRefN) {
      h = (HoistedValueRef *)h->value;
    }
    return h->value;
  }
  case AST::BinaryN: {
    auto b = (Binary *)e;
    switch (b->op) {
    case Binary::NOT: {
      auto v = interpret(b->right);
      e->set(!v->asLogical());
      break;
    }
    case Binary::LOOKUP: {
      auto sym = Symbol::findSymbol(interpret(b->right)->asString().getText());
      e->set(sym->getValue());
      break;
    }
    case Binary::CONCAT: {
      bool isList = true;
      vector<Value *> leaves;
      e->walkConcat([this, &isList, &leaves](Expression *e) {
        leaves.push_back(interpret(e));
        if (leaves.back()->kind != Value::List) {
          isList = false;
        }
      });
      if (isList) {
        e->clearList();
        for (auto leaf : leaves) {
          for (auto &v : leaf->list) {
            e->append(v);
          }
        }
      } else {
        stringstream str;
        unsigned flags = 0;
        for (auto leaf : leaves) {
          auto &s = leaf->asString();
          str << s.getText();
          flags |= s.getFlags();
        }
        e->set(StringRef(str.str(), flags));
      }
      break;
    }
    case Binary::MATCH: {
      auto r = interpret(b->right)->getRegEx();
      auto target = interpret(b->left)->asString().getText();
      e->set(interpretMatch(r, target));
      break;
    }
    case Binary::REPLACE: {
      auto m = (Binary *)b->left;
      assert(m->isOp(m->MATCH));
      auto r = interpret(m->right)->getRegEx();
      auto input = interpret(m->left)->asString().getText();
      auto replacement = interpret(b->right)->asString().getText();
      b->set(regEx->replace(r, replacement, input));
      break;
    }
    case Binary::SET_GLOBAL: {
      auto sc = interpret(b->right)->asString();
      sc.setIsGlobal();
      b->set(sc);
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
    case Binary::SUBSCRIPT: {
      auto list = interpret(b->left);
      auto index = (int)interpret(b->right)->asNumber();
      if (index < 0) {
        throw Exception("negative index in subscript");
      }
      if (list->kind == list->List) {
        if (index >= list->list.size()) {
          b->set(StringRef("", 0));
        } else {
          b->set(&list->list[index]);
        }
      } else {
        auto &str = list->asString().getText();
        if (index >= str.length()) {
          b->set(StringRef("", 0));
        } else {
          string temp;
          temp.append(1, str[index]);
          b->set(StringRef(temp, 0));
        }
      }
      break;
    }
    }
  } break;
  }
  return e;
}
