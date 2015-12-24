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
using std::string;
using std::vector;
using std::stringstream;

namespace {
class Dumper {
  std::ostream &OS;
  void indent(int depth);
  void ifstmt(int depth, const IfStatement *i);
  void end(int depth, int id);

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

void Dumper::end(int depth, int id) {
  OS << std::setw(3) << id << ' ';
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
  case AST::RewindN:
  case AST::InputN:
  case AST::OutputN: {
    auto i = static_cast<const IOStmt *>(node);
    indent(depth);

    OS << (isa<Input>(i) ? "input " : isa<Output>(i) ? "output " : "rewind ");
    dumpExpr(i->buffer);
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
  }
}

void Dumper::dump(int depth, const Statement *node) {
  do {
    dumpOneStmt(depth, node);
    switch (node->kind()) {
    case AST::ForeachN: {
      auto f = static_cast<const Foreach *>(node);
      dump(depth + 1, f->body);
      end(depth, node->getId());
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
    OS << ' ' << b->opName(b->op) << ' ';
    dumpExpr(b->right);
    if (b->op == b->LOOKUP) {
      OS << ")";
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
  case AST::RegExReferenceN: {
    auto r = static_cast<const RegExReference *>(node);
    OS << "regex[" << r->getIndex() << "]";
    break;
  }
  case AST::ArgN: {
    assert("should not reach ArgN");
    break;
  }
  }
}

void Dumper::ifstmt(int depth, const IfStatement *i) {
  auto id = i->getId();
  for (;;) {
    dump(depth + 1, i->thenStmts);
    auto e = static_cast<const Statement *>(i->elseStmts);
    if (!e) {
      break;
    }
    if (e->kind() != AST::IfStmtN || e->getNext()) {
      OS << std::setw(3) << id << ' ';
      indent(depth);
      OS << "else\n";
      dump(depth + 1, e);
      break;
    }
    i = static_cast<const IfStatement *>(e);
    id = i->getId();
    dumpOneStmt(depth, i, /*elseIf*/ true);
  }
  end(depth, id);
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
  case AST::RegExReferenceN:
  case AST::RegExPatternN:
  case AST::ControlN:
    return Value::String;
  case AST::CallN:
    return BuiltinCalls::callKind(((Call *)this)->getCallId());
  case AST::IntegerN:
    return Value::Number;
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

static std::regex variable(R"((\\\$)|(\$[a-zA-Z][a-zA-Z0-9_]+)|(\$[0-9]+))");

static Expression *expandVariables(const StringRef &text) {

  if (text.isRaw()) {
    return new StringConst(text);
  }
  const char *ctext = text.getText().c_str();
  size_t len = text.getText().length();
  auto vars_begin = std::cregex_iterator(ctext, ctext + len, variable);
  auto vars_end = std::cregex_iterator();
  if (vars_begin == vars_end) {
    return new StringConst(text);
  }

  vector<Expression *> terms;
  stringstream str;
  int strLength = 0;
  unsigned numStringCounts = 0;
  const char *last = ctext;
  for (std::cregex_iterator vars = vars_begin; vars != vars_end; ++vars) {
    auto match = (*vars)[0];
    const char *cvar = match.first;
    str.write(last, cvar - last);
    strLength += (cvar - last);
    if (*cvar == '\\') {
      str << '$';
      strLength += 1;
    } else {
      if (strLength > 0) {
        numStringCounts += 1;
        terms.push_back(new StringConst(StringRef(str.str(), text.getFlags())));
        str.str("");
        strLength = 0;
      }
      if (isdigit(cvar[1])) {
        stringstream temp;
        temp.write(cvar + 1, match.second - cvar - 1);
        unsigned i;
        temp >> i;
        terms.push_back(new VarMatch(i));
      } else {
        auto sym =
            Symbol::findSymbol(string(cvar + 1, match.second - cvar - 1));
        terms.push_back(new Variable(*sym));
      }
    }
    last = match.second;
  }
  auto clast = ctext + len;
  if (last != clast) {
    str.write(last, clast - last);
    strLength += clast - last;
  }
  if (strLength > 0 || numStringCounts == 0) {
    terms.push_back(new StringConst(StringRef(str.str(), text.getFlags())));
  }
  auto e = terms.back();
  for (auto i = terms.size() - 1; i > 0;) {
    i -= 1;
    e = new Binary(Binary::CONCAT, terms[i], e, 0);
  }
  return e;
}

Expression *AST::stringConst(string *constant) {
  return expandVariables(StringRef(constant));
}
