//
//  AST.cpp
//  rsed
//
//  Created by David Callahan on 6/25/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include <iostream>
#include "RegEx.h"
#include "AST.h"

namespace {
class Dumper {
  std::ostream &OS;
  void indent(int depth);
  void ifstmt(int depth, const IfStatement *i);

public:
  Dumper(std::ostream &OS) : OS(OS){};
  void dump(int depth, const Statement *node);
  void dumpExpr(const Expression *node);
};
}

void AST::dump() const {
  Dumper d(std::cout);
  std::cout << "style " << RegEx::getStyleName() << '\n';
  if (isStatement()) {
    d.dump(0, (Statement *)this);
  } else {
    d.dumpExpr((Expression *)this);
  }
}

void Dumper::dump(int depth, const Statement *node) {
  do {
    switch (node->kind()) {
    case AST::ForeachN: {
      auto f = static_cast<const Foreach *>(node);
      indent(depth);
      OS << "foreach ";
      if (auto c = f->getControl()) {
        dumpExpr(c);
      }
      OS << '\n';
      dump(depth + 1, f->getBody());
      indent(depth);
      OS << "end\n";
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
      ifstmt(depth, i);
      break;
    }
    case AST::SetN: {
      auto s = static_cast<const Set *>(node);
      indent(depth);
      OS << "set $" << s->getName() << " = ";
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
    node = static_cast<const Statement *>(node)->getNext();
  } while (node);
}

void Dumper::dumpExpr(const Expression *node) {
  if (!node) {
    OS << "<nullptr>";
    return;
  }
  bool addParens = node->getNext();
  if (addParens) {
    OS << "( ";
  }
  for (;;) {
    switch (node->kind()) {
    case AST::PatternN: {
      auto p = static_cast<const Pattern *>(node);
      OS << p->getPattern();
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
    case AST::NotN: {
      auto n = static_cast<const NotExpr *>(node);
      OS << "not ";
      dumpExpr(n->getPattern());
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
    case AST::MatchN: {
      auto m = static_cast<const Match *>(node);
      dumpExpr(m->getPattern());
      OS << " =~ ";
      dumpExpr(m->getTarget());
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
    default:
      OS << "bad-expr-kind=" << int(node->kind());
      return;
    }
    node = node->getNext();
    if (!node) {
      break;
    }
    OS << ' ';
  }
  if (addParens) {
    OS << " )";
  }
}

void Dumper::ifstmt(int depth, const IfStatement *i) {
  const char *keywords = "if ";
  for (;;) {
    indent(depth);
    OS << keywords;
    dumpExpr(i->getPattern());
    OS << " then\n";
    dump(depth + 1, i->getThenStmts());
    auto e = static_cast<const Statement *>(i->getElseStmts());
    if (!e) {
      break;
    }
    if (e->kind() != AST::IfStmtN || e->getNext()) {
      dump(depth + 1, e);
      break;
    }
    i = static_cast<const IfStatement *>(e);
    keywords = "else if ";
  }
  indent(depth);
  OS << "end\n";
}

void Dumper::indent(int depth) {
  while (depth--) {
    OS << "   ";
  }
}
