//
//  AST.cpp
//  rsed
//
//  Created by David Callahan on 6/25/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include "AST.h"
#include <iostream>
#include <iomanip>
#include <assert.h>
#include "ASTWalk.h"
#include "RegEx.h"
#include "BuiltinCalls.h"
#include "ExpandVariables.h"
using std::string;
using std::vector;
using std::stringstream;

namespace {
class Dumper {
  std::ostream &OS;
  void indent(int depth);
  void ifstmt(int depth, const IfStatement *i);
  void end(int depth, int id, int line);

public:
  Dumper(std::ostream &OS) : OS(OS){};
  void dump(int depth, const Statement *node);
  void dumpExpr(const Expression *node);
  void dumpOneStmt(int depth, const Statement *node, bool elseIf = false);
};
}

int AST::next_id = 0;
int RegExPattern::nextIndex = 0;
const char *const AST::CURRENT_LINE_SYM = "CURRENT";

void AST::dump() const {
  Dumper d(std::cout);
  std::cout << "style " << RegEx::getStyleName() << '\n';
  if (isStatement()) {
    d.dump(0, (Statement *)this);
  } else {
    d.dumpExpr((Expression *)this);
    std::cout << '\n';
  }
}

void Statement::dumpOne() {
  Dumper d(std::cout);
  d.dumpOneStmt(0, this);
}

void Dumper::end(int depth, int id, int line) {
  OS << std::setw(3) << id << ' ';
  OS << std::setw(3) << line << ' ';
  indent(depth);
  OS << "end\n";
}

void Dumper::dumpOneStmt(int depth, const Statement *node, bool elseIf) {
  OS << std::setw(3) << node->getId() << ' ';
  OS << std::setw(3) << node->getSourceLine() << ' ';
  switch (node->kind()) {
  case AST::ForeachN: {
    auto f = static_cast<const Foreach *>(node);
    indent(depth);
    OS << "foreach ";
    if (auto c = f->control) {
      dumpExpr(c);
    }
    OS << '\n';
    break;
  }
  case AST::CopyN: {
    indent(depth);
    OS << "copy\n";
    break;
  }
  case AST::SkipN: {
    indent(depth);
    OS << "skip\n";
    break;
  }
  case AST::ReplaceN: {
    auto r = static_cast<const Replace *>(node);
    indent(depth);
    OS << "replace ";
    dumpExpr(r->pattern);
    OS << ' ';
    dumpExpr(r->replacement);
    OS << '\n';
    break;
  }
  case AST::SplitN: {
    auto s = static_cast<const Split *>(node);
    OS << "split ";
    if (s->target) {
      dumpExpr(s->target);
      OS << ' ';
    }
    OS << "with ";
    dumpExpr(s->separator);
    OS << '\n';
    break;
  }
  case AST::IfStmtN: {
    auto i = static_cast<const IfStatement *>(node);
    indent(depth);
    OS << (elseIf ? "else if " : "if ");
    dumpExpr(i->predicate);
    OS << " then\n";
    break;
  }
  case AST::SetN: {
    auto s = static_cast<const Set *>(node);
    indent(depth);
    dumpExpr(s->lhs);
    OS << " = ";
    dumpExpr(s->rhs);
    OS << '\n';
    break;
  }
  case AST::PrintN: {
    auto p = static_cast<const Print *>(node);
    indent(depth);
    OS << "print ";
    dumpExpr(p->text);
    if (auto b = p->buffer) {
      OS << " to ";
      dumpExpr(b);
    }
    OS << '\n';
    break;
  }
  case AST::ErrorN: {
    auto e = static_cast<const Error *>(node);
    indent(depth);
    OS << "error ";
    dumpExpr(e->text);
    OS << '\n';
    break;
  }
  case AST::ColumnsN: {
    auto c = static_cast<const Columns *>(node);
    indent(depth);
    OS << "columns ";
    dumpExpr(c->columns);
    OS << '\n';
    break;
  }
  case AST::CloseN:
  case AST::InputN:
  case AST::OutputN: {
    auto i = static_cast<const IOStmt *>(node);
    indent(depth);

    if (auto c = isa<Close>(i)) {
      OS << "close ";
      switch (c->getMode()) {
        case Close::Input:
          OS << "input" ;
          break;
        case Close::Output:
          OS << "output";
          break;
        case Close::ByName:
          dumpExpr(c->buffer);
          break;
      }
    }
    else {
      OS << (isa<Input>(i) ? "input " : "output " );
      dumpExpr(i->buffer);
    }
    OS << '\n';
    break;
  }
  case AST::RequiredN: {
    auto r = static_cast<const Required *>(node);
    indent(depth);
    OS << "require ";
    if (r->predicate) {
      dumpExpr(r->predicate);
    } else {
      OS << r->getCount();
    }
    OS << '\n';
  }
  case AST::HoistedValueN: {
    auto h = static_cast<const HoistedValue *>(node);
    indent(depth);
    OS << "hoisted " << h->rhs->getId() << ' ';
    dumpExpr(h->rhs);
    OS << '\n';
    break;
  }
  }
}

void Dumper::dump(int depth, const Statement *node) {
  do {
    dumpOneStmt(depth, node);
    switch (node->kind()) {
    case AST::ForeachN: {
      auto f = static_cast<const Foreach *>(node);
      dump(depth + 1, f->body);
      end(depth, node->getId(), node->getSourceLine());
      break;
    }
    case AST::IfStmtN: {
      auto i = static_cast<const IfStatement *>(node);
      ifstmt(depth, i);
      break;
    }
    default:
      break;
    }
    node = static_cast<const Statement *>(node)->getNext();
  } while (node);
}

const char *Expression::opName(Operators op) {
  switch (op) {
  case ADD:
    return "+";
  case NEG:
  case SUB:
    return "-";
  case MUL:
    return "*";
  case DIV:
    return "/";
  case MATCH:
    return "=~";
  case REPLACE:
    return "replace";
  case CONCAT:
    return " ";
  case LT:
    return "<";
  case LE:
    return "<=";
  case EQ:
    return "==";
  case NE:
    return "!=";
  case GE:
    return ">=";
  case GT:
    return ">";
  case NOT:
    return "not";
  case AND:
    return "and";
  case OR:
    return "or";
  case LOOKUP:
    return "$(";
  case SET_GLOBAL:
    return "global";
  }
}

void Dumper::dumpExpr(const Expression *node) {
  if (!node) {
    OS << "<nullptr>";
    return;
  }
  switch (node->kind()) {
  case AST::BinaryN: {
    auto b = static_cast<const Binary *>(node);
    if (b->left) {
      dumpExpr(b->left);
    }
    if (b->isOp(b->CONCAT)) {
      OS << ' ';
      auto r = b->right;
      if (r->kind() == r->BinaryN) {
        OS << '(';
      }
      dumpExpr(r);
      if (r->kind() == r->BinaryN) {
        OS << ')';
      }
    } else {
      OS << ' ' << b->opName(b->op) << ' ';
      dumpExpr(b->right);
      if (b->op == b->LOOKUP) {
        OS << ")";
      }
    }
    break;
  }
  case AST::ControlN: {
    auto c = static_cast<const Control *>(node);
    if (c->hasLimit()) {
      OS << c->getLimit();
      OS << ' ';
    }
    if (auto p = c->pattern) {
      OS << (c->getStopKind() == AST::StopAt ? "to " : "past ");
      dumpExpr(p);
    }
    break;
  }
  case AST::IntegerN: {
    auto i = static_cast<const Integer *>(node);
    OS << i->getValue();
    break;
  }
  case AST::VarMatchN: {
    auto i = static_cast<const VarMatch *>(node);
    OS << '$' << i->getValue();
    break;
  }
  case AST::StringConstN: {
    OS << static_cast<const StringConst *>(node)->getConstant();
    break;
  }
  case AST::VariableN: {
    OS << '$' << static_cast<const Variable *>(node)->getName();
    break;
  }
  case AST::CallN: {
    auto c = static_cast<const Call *>(node);
    OS << c->getName() << '(';
    for (auto a = c->args; a; a = a->nextArg) {
      dumpExpr(a->value);
      if (a->nextArg) {
        OS << ", ";
      }
    }
    OS << ')';
    break;
  }
  case AST::RegExPatternN: {
    auto r = static_cast<const RegExPattern *>(node);
    OS << "regex[" << r->getIndex() << ",";
    dumpExpr(r->pattern);
    OS << "]";
    break;
  }
  case AST::ArgN: {
    assert("should not reach ArgN");
    break;
  }
  case AST::HoistedValueRefN: {
    auto h = (HoistedValueRef *)node;
    OS << "hoisted[" << h->value->getId() << "]";
    break;
  }
  }
}

void Dumper::ifstmt(int depth, const IfStatement *i) {
  for (;;) {
    dump(depth + 1, i->thenStmts);
    auto e = static_cast<const Statement *>(i->elseStmts);
    if (!e) {
      break;
    }
    if (e->kind() != AST::IfStmtN || e->getNext()) {
      OS << std::setw(3) << i->getId() << ' ';
      indent(depth);
      OS << "else\n";
      dump(depth + 1, e);
      break;
    }
    i = static_cast<const IfStatement *>(e);
    dumpOneStmt(depth, i, /*elseIf*/ true);
  }
  end(depth, i->getId(), i->getSourceLine());
}

void Dumper::indent(int depth) {
  while (depth--) {
    OS << "   ";
  }
}

Value::Kind Expression::valueKind() {
  switch (kind()) {
  case AST::StringConstN:
  case AST::VariableN:
  case AST::VarMatchN:
  case AST::ArgN:
  case AST::RegExPatternN:
  case AST::ControlN:
    return Value::String;
  case AST::CallN:
    return BuiltinCalls::callKind(((Call *)this)->getCallId());
  case AST::IntegerN:
    return Value::Number;
  case AST::HoistedValueRefN:
    return ((HoistedValueRef *)this)->value->valueKind();
  case AST::BinaryN:
    switch (((Binary *)this)->op) {
    case Binary::ADD:
    case Binary::SUB:
    case Binary::MUL:
    case Binary::DIV:
    case Binary::NEG:
      return Value::Number;
    case Binary::LT:
    case Binary::LE:
    case Binary::EQ:
    case Binary::NE:
    case Binary::GE:
    case Binary::GT:
    case Binary::NOT:
    case Binary::AND:
    case Binary::OR:
    case Binary::MATCH:
      return Value::Logical;
    case Binary::REPLACE:
    case Binary::CONCAT:
    case Binary::LOOKUP:
    case Binary::SET_GLOBAL:
      return Value::String;
    }
  }
}

Expression *AST::checkPattern(Expression *pattern) {
  if (auto n = pattern->isOp(Expression::NOT)) {
    n->right = checkPattern(n->right);
    return n;
  }
  if (pattern->valueKind() == Value::Logical) {
    return pattern;
  }
  return match(current(), pattern, pattern->getSourceLine());
}

namespace {
class StaticExpandVariables : public ExpandVariables {
public:
  Expression *term = nullptr;
  void append(Expression *t) {
    term = (term ? new Binary(Binary::CONCAT, term, t, 0) : t);
  }
  void single(const std::string &s, unsigned flags) override {
    term = new StringConst(StringRef(s, flags));
  }
  void string(stringstream &s, unsigned flags) override {
    append(new StringConst(StringRef(s.str(), flags)));
  }
  void varMatch(unsigned i) override { append(new VarMatch(i)); }
  void variable(std::string name) override {
    auto sym = Symbol::findSymbol(name);
    append(new Variable(*sym));
  }
};
}

static Expression *expandVariables(const StringRef &text) {

  StaticExpandVariables expander;
  expander.expand(text);
  return expander.term;
}

Expression *AST::stringConst(string *constant) {
  return expandVariables(StringRef(constant));
}

Statement *AST::input(Expression *s, int sourceLine) {
  bool isShell = false;
  auto c = (Call *)s;
  if (c->kind() == c->CallN && c->getCallId() == BuiltinCalls::SHELL &&
      c->args) {
    isShell = true;
    auto args = c->args;
    delete c;
    s = args->value;
    for (;;) {
      auto next = args->nextArg;
      delete args;
      if (!next) {
        break ;
      }
      args = next;
      s = new Binary(Binary::CONCAT, s, new StringConst(" "), sourceLine);
      s = new Binary(Binary::CONCAT, s, args->value, sourceLine);
    }
  }
  return new Input(s,isShell, sourceLine);
}
