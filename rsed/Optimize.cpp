//
//  Optimize.cpp
//  rsed
//
//  Created by David Callahan on 12/14/15.
//
//

// TODO -- compiler suitable regular expressions

#include <gflags/gflags.h>
#include "Optimize.h"
#include "AST.h"
#include <unordered_set>
#include <assert.h>
using std::unordered_set;

DEFINE_bool(optimize, false, "enable optimizer");

namespace {

typedef std::pair<bool, unsigned> HoistInfo;
bool isInvariant(HoistInfo h) { return h.first; }
// unsigned cost(HoistInfo h) { return h.second; }
bool shouldHoist(HoistInfo h) { return h.first && h.second > 0; }
HoistInfo operator+(HoistInfo h1, HoistInfo h2) {
  h1.first &= h2.first;
  h1.second += h2.second;
  return h1;
}
HoistInfo operator+(HoistInfo h1, unsigned k) {
  h1.second += k;
  return h1;
}
class Optimizer {
  unordered_set<Symbol *> setInLoop;
  void noteSetVariables(Statement *body);
  bool isInvariant(Symbol &sym) { return setInLoop.count(&sym); }
  Statement *firstInvariant, *lastInvariant;
  void hoistInvariants(Statement *body);
  Expression *hoistInvariants(Expression *expr);
  void hoistInvariants(Expression**expr) {
    *expr = hoistInvariants(*expr);
  }
  HoistInfo checkHoist(Expression *expr);
  HoistInfo checkHoistTerm(Expression *expr);
  Expression *hoist(Expression *expr);

  template <typename ACTION> void processFields(Expression *, ACTION &action);

public:
  Optimizer() {}
  Statement *optimize(Statement *input);
};
}

namespace Optimize {
Statement *optimize(Statement *input) {
  if (!FLAGS_optimize) {
    return input;
  }
  Optimizer opt;
  return opt.optimize(input);
}
}

Statement *Optimizer::optimize(Statement *input) {

  if (!input) {
    return input;
  }
  input->setNext(optimize(input->getNext()));
  if (auto foreach = isa<Foreach>(input)) {
    auto newBody = optimize(foreach->body);
    noteSetVariables(newBody);
    hoistInvariants(newBody);
    foreach->body = newBody;
    if (lastInvariant) {
      lastInvariant->setNext(foreach);
      return firstInvariant;
    }
    return foreach;
  }
  if (auto ifstmt = isa<IfStatement>(input)) {
    ifstmt->setThenStmts(optimize(ifstmt->getThenStmts()));
    ifstmt->setElseStmts(optimize(ifstmt->getElseStmts()));
    return ifstmt;
  }
  return input;
}

void Optimizer::noteSetVariables(Statement *body) {
#if 0
  setInLoop.clear();
  body->walk([this](Statement *stmt) {
    if (auto set = isa<Set>(stmt)) {
      setInLoop.insert(&set->getSymbol());
    }
    return AST::ContinueW;
  });
#endif
}

#define APPLY(T, F, A, x) ((T *)x)->set##F(A(((T *)x)->get##F()))
#define HOIST(T, F) APPLY(T, F, hoistInvariants, stmt)

// #define HOIST(T, F) ((T *)stmt)->set##F(hoistInvariants(((T
// *)stmt)->get##F()))
void Optimizer::hoistInvariants(Statement *body) {
#if 0
  firstInvariant = lastInvariant = nullptr;
  body->walk([this](Statement *stmt) {
    switch (stmt->kind()) {
    case AST::ForeachN:
      hoistInvariants(& ForeachP(stmt)->control);
      break;
    case AST::ReplaceN:
      HOIST(Replace, Pattern);
      HOIST(Replace, Replacement);
      break;
    case AST::IfStmtN:
      HOIST(IfStatement, Pattern);
      break;
    case AST::SetN:
      HOIST(Set, Rhs);
      break;
    case AST::ColumnsN:
      HOIST(Columns, InExpr);
      break;
    case AST::PrintN:
      HOIST(Print, Text);
      break;
    case AST::SkipN:
    case AST::CopyN:
    case AST::ErrorN:
    case AST::InputN:
    case AST::OutputN:
      break;
    }
    return AST::ContinueW;
  });
#endif
}

Expression *Optimizer::hoistInvariants(Expression *expr) {
  return (shouldHoist(checkHoist(expr)) ? hoist(expr) : expr);
}

HoistInfo Optimizer::checkHoist(Expression *expr) {

#if 0
  if (expr->kind() != expr->BinaryN) {
    return checkHoistTerm(expr);
  }
  assert(BinaryP(expr)->op == expr->CONCAT);

  // first of an implicit concatentation
  assert(expr->kind() != Expression::ArgN);
  std::vector<Expression *> terms;
  std::vector<HoistInfo> hterms;
  for (auto t = expr; t; t = t->getNext()) {
    terms.push_back(t);
    hterms.push_back(checkHoistTerm(t));
  }
  auto N = terms.size();
  size_t first = N;
  Expression *last = nullptr;
  for (auto i = terms.size(); i > 0;) {
    i -= 1;
    if (::isInvariant(hterms[i])) {
      if (first == N) {
        first = i;
      }
      continue;
    }
    if (first != N) {
      // i+1..first inclusive are all invariant....
      Expression *cur = terms[i + 1];
      if (i + 1 != first || shouldHoist(hterms[first])) {
        // truncate the list at 'first'
        terms[first]->setNext(nullptr);
        cur = hoist(cur);
      }
      last = AST::list(cur, last);
      first = N;
    }
    last = AST::list(terms[i], last);
  }
  if (first == N - 1) {
    // entire expression is invariant
    return HoistInfo(true, N + 1);
  }
  if (first != N) {
    // i+1..first inclusive are all invariant....
    Expression *cur = terms[0];
    if (1 != first || shouldHoist(hterms[first])) {
      // truncate the list at 'first'
      terms[first]->setNext(nullptr);
      cur = hoist(cur);
    }
    last = AST::list(cur, last);
  }
  // what do we do here? I guesss we have
  // to move change how this is handled
  //   --maybe add a wrapper Concat node
  assert("not yet finished");
#endif
  return HoistInfo(false, 0);
}

HoistInfo Optimizer::checkHoistTerm(Expression *expr) {

#if 0
  switch (expr->kind()) {
  case AST::BinaryN:
    assert("not yet implemented");
    break;
  case AST::ControlN:
    APPLY(Control, Pattern, hoistInvariants, expr);
    break;
  case AST::BufferN:
    APPLY(Buffer, FileName, hoistInvariants, expr);
    break;
  case AST::MatchN:
    // TODO -- we could hoist this if we knew there
    // were no dynamic field references ($0, $1,....)
    APPLY(Match, Pattern, hoistInvariants, expr);
    APPLY(Match, Target, hoistInvariants, expr);
    break;
  case AST::ArgN:
    return checkHoist(((Arg *)expr)->value);

  case AST::VariableN:
    return HoistInfo(isInvariant(((Variable *)expr)->getSymbol()), 0);
  case AST::IntegerN:
    return HoistInfo(true, 0);
  case AST::StringConstN:
    // ok, what is the impact of interpretation?
    if (((StringConst *)expr)->getConstant().getText().find('$') !=
        std::string::npos) {
      break;
    }
    return HoistInfo(true, 0);

  case AST::CallN: {
    auto *c = (Call *)expr;
    std::vector<HoistInfo> args;
    bool invariant = true;
    for (auto a = c->args; a; a =a->nextArg) {
      args.push_back(checkHoist(((Arg *)a)->value));
      if (!::isInvariant(args.back())) {
        invariant = false;
      }
    }
    if (invariant) {
      return HoistInfo(true, 2);
    }
    auto ap = args.begin();
    for (auto a = c->args; a; a = a->nextArg, ++ap) {
      if (shouldHoist(*ap)) {
        a->value = hoist(a->value);
      }
    }
    break;
  }
  }
#endif
  return HoistInfo(false, 0);
}

Expression *Optimizer::hoist(Expression *expr) { return expr; }
