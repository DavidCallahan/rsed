//
//  AST.cpp
//  rsed
//
//  Created by David Callahan on 6/25/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include <iostream>
#include <iomanip>
#include "RegEx.h"
#include "AST.h"
#include "BuiltinCalls.h"
#include <assert.h>

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
    dumpExpr(r->getPattern());
    OS << ' ';
    dumpExpr(r->getReplacement());
    OS << '\n';
    break;
  }
  case AST::IfStmtN: {
    auto i = static_cast<const IfStatement *>(node);
    indent(depth);
    OS << (elseIf ? "else if " : "if ");
    dumpExpr(i->getPattern());
    OS << " then\n";
    break;
  }
  case AST::SetN: {
    auto s = static_cast<const Set *>(node);
    indent(depth);
    OS << "$" << s->getName() << " = ";
    dumpExpr(s->getRhs());
    OS << '\n';
    break;
  }
  case AST::PrintN: {
    auto p = static_cast<const Print *>(node);
    indent(depth);
    OS << "print ";
    dumpExpr(p->getText());
    if (auto b = p->getBuffer()) {
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
    dumpExpr(e->getText());
    OS << '\n';
    break;
  }
  case AST::ColumnsN: {
    auto c = static_cast<const Columns *>(node);
    indent(depth);
    OS << "columns ";
    dumpExpr(c->getColumns());
    if (c->getInExpr()) {
      OS << " in ";
      dumpExpr(c->getInExpr());
    }
    OS << '\n';
    break;
  }
  case AST::InputN:
  case AST::OutputN: {
    auto i = static_cast<const Input *>(node);
    indent(depth);
    OS << (node->kind() == AST::InputN ? "input " : "output ");
    dumpExpr(i->getBuffer());
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

const char *AST::opName(AST::Operators op) {
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
    OS << b->opName(b->op);
    dumpExpr(b->right);
    break;
  }
  case AST::ControlN: {
    auto c = static_cast<const Control *>(node);
    if (c->hasLimit()) {
      OS << c->getLimit();
      OS << ' ';
    }
    if (auto p = c->getPattern()) {
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
  case AST::StringConstN: {
    OS << static_cast<const StringConst *>(node)->getConstant();
    break;
  }
  case AST::VariableN: {
    OS << '$' << static_cast<const Variable *>(node)->getName();
    break;
  }
  case AST::BufferN: {
    auto b = static_cast<const Buffer *>(node);
    if (auto f = b->getFileName()) {
      dumpExpr(f);
    } else {
      OS << b->getBufferName();
    }
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
  case AST::ArgN: {
    assert("should not reach ArgN");
    break;
  }
  }
}

void Dumper::ifstmt(int depth, const IfStatement *i) {
  auto id = i->getId();
  for (;;) {
    dump(depth + 1, i->getThenStmts());
    auto e = static_cast<const Statement *>(i->getElseStmts());
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

std::string AST::checkPattern(Expression *pattern) {
  std::string msg;
  pattern->walkDown([&msg](Expression *e) {
    if (e->isOp(MATCH)) {
      msg = "invalid =~ in expression";
      return StopW;
    }
    return ContinueW;
  });
  return msg;
}
std::string AST::checkTopExpression(Expression *pattern) {
  std::string msg;
  pattern->walkDown([&msg](Expression *e) {
    if (e->isOp(NOT)) {
      msg = "invalid NOT in expression";
      return StopW;
    } else if (e->isOp(MATCH)) {
      msg = "invalid MATCH in expression";
      return StopW;
    }
    return ContinueW;
  });
  return msg;
}
