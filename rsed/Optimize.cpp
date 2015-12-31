//
//  Optimize.cpp
//  rsed
//
//  Created by David Callahan on 12/14/15.
//
//

#include <unordered_set>
#include <assert.h>
#include <gflags/gflags.h>
#include "rsed.h"
#include "Optimize.h"
#include "AST.h"
#include "ASTWalk.h"
#include "BuiltinCalls.h"
using std::unordered_set;
using std::vector;

// DEFINE_bool(no_optimize, false, "disable optimizer");
DEFINE_bool(optimize, true, "enable optimizer");
namespace {

const unsigned HOISTABLE_COST = 1;
typedef std::pair<bool, unsigned> HoistInfo;
const HoistInfo NON_HOISTABLE{false, 0};
const HoistInfo HOISTABLE{true, HOISTABLE_COST};

bool isInvariant(HoistInfo h) { return h.first; }
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
  bool unknownSymbolSet;
  unordered_set<Symbol *> setInLoop;
  void noteSetVariables(Statement *body);
  bool isInvariant(Symbol *sym) {
    return !unknownSymbolSet && !sym->isDynamic() && !setInLoop.count(sym);
  }
  Statement *firstInvariant, *lastInvariant;
  void hoistInvariants(Statement *body);
  HoistInfo checkHoist(Expression **expr);
  HoistInfo checkHoistConcat(Expression **expr);
  void hoist(Expression **expr);
  void hoistInvariants(Expression **expr);

public:
  Optimizer() {}
  Statement *optimize(Statement *input);
};
}

namespace Optimize {
Statement *optimize(Statement *input) {
  if (FLAGS_optimize) {
    return input;
  }
  Optimizer opt;
  auto out = opt.optimize(input);
  if (RSED_Debug::dump) {
    out->dump();
  }
  return out;
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
    firstInvariant = lastInvariant = nullptr;
    hoistInvariants(&foreach->control);
    hoistInvariants(newBody);
    foreach
      ->body = newBody;
    if (lastInvariant) {
      lastInvariant->setNext(foreach);
      return firstInvariant;
    }
    return foreach;
  }
  if (auto ifstmt = isa<IfStatement>(input)) {
    ifstmt->thenStmts = optimize(ifstmt->thenStmts);
    ifstmt->elseStmts = optimize(ifstmt->elseStmts);
    return ifstmt;
  }
  return input;
}

void Optimizer::noteSetVariables(Statement *body) {
  setInLoop.clear();
  unknownSymbolSet = false;
  body->walk([this](Statement *stmt) {
    if (auto set = isa<Set>(stmt)) {
      auto lhs = set->lhs;
      if (lhs->kind() == lhs->VariableN) {
        auto sym = &((Variable *)lhs)->getSymbol();
        setInLoop.insert(sym);
      } else {
        unknownSymbolSet = true;
        return AST::StopW;
      }
    }
    return AST::ContinueW;
  });
}

void Optimizer::hoistInvariants(Statement *body) {
  body->applyExprs([this](Expression *&expr) { hoistInvariants(&expr); });
}

void Optimizer::hoistInvariants(Expression **expr) {
  if (*expr && shouldHoist(checkHoist(expr))) {
    hoist(expr);
  }
}

HoistInfo Optimizer::checkHoistConcat(Expression **exprHome) {

  auto expr = *exprHome;
  assert(BinaryP(expr)->op == expr->CONCAT);

  // first of an implicit concatentation
  assert(expr->kind() != Expression::ArgN);

  typedef std::pair<Expression *, HoistInfo> TermInfo;
  std::vector<TermInfo> terms;
  std::vector<Binary *> operators;
  bool allInvariant = true;
  bool hasInvariant = false;
  expr->walkConcat([this, &terms, &hasInvariant, &allInvariant](Expression *t) {
    terms.push_back(std::make_pair(t, checkHoist(&t)));
    if (::isInvariant(terms.back().second)) {
      hasInvariant = true;
    } else {
      allInvariant = false;
    }
  }, &operators);

  if (allInvariant) {
    return HOISTABLE;
  }
  if (!hasInvariant) {
    return NON_HOISTABLE;
  }

  auto ops = operators.begin();
  // rebuild a concat from one of the input operators
  auto makeOp = [&ops](Expression *left, Expression *right) -> Binary *{
    auto b = *ops++;
    b->left = left;
    b->right = right;
    return b;
  };

  // rebuild the tree, collecting sequences of invaraint
  // terms so they can be hoisted.
  Expression *result = nullptr;
  Expression *invariant = nullptr;
  bool doHoist = false; // should hoist invariant term
  for (auto &term : terms) {
    auto e = term.first;
    if (::isInvariant(term.second)) {
      // true for any two or more invariant terms or one shouldHoist term
      doHoist = doHoist || invariant || shouldHoist(term.second);
      invariant = (invariant ? makeOp(invariant, e) : e);
    } else {
      if (invariant) {
        auto b = makeOp(invariant, e);
        if (doHoist) {
          hoist(&b->left);
        }
        e = b;
        invariant = nullptr;
        doHoist = false;
      }
      result = (result ? makeOp(result, e) : e);
    }
  }
  if (invariant) {
    assert(result && "unepected invariant expression");
    auto b = makeOp(result, invariant);
    if (doHoist) {
      hoist(&b->right);
    }
    result = b;
  }
  *exprHome = result;
  return NON_HOISTABLE;
}

HoistInfo Optimizer::checkHoist(Expression **exprHome) {

  auto expr = *exprHome;
  switch (expr->kind()) {
  case AST::VarMatchN:
    break;
  case AST::RegExPatternN:
    return checkHoist(&((RegExPattern *)expr)->pattern) + HOISTABLE_COST;
  case AST::ControlN:
    hoistInvariants(&((Control *)expr)->pattern);
    break;
  case AST::ArgN:
    return checkHoist(&((Arg *)expr)->value);
  case AST::VariableN: {
    auto sym = &((Variable *)expr)->getSymbol();
    return HoistInfo(isInvariant(sym), 0);
  }
  case AST::LogicalN:
  case AST::NumberN:
  case AST::StringConstN:
    return HoistInfo(true, 0);
  case AST::HoistedValueRefN:
    assert(0 && "unexpected hoistvalueref type");
    break;
  case AST::ListN:
  case AST::CallN: {
    std::vector<HoistInfo> args;
    bool invariant = true;
    for (auto a = ListP(expr)->head; a; a = a->nextArg) {
      args.push_back(checkHoist(&((Arg *)a)->value));
      if (!::isInvariant(args.back())) {
        invariant = false;
      }
    }
    if (invariant && (expr->kind() != expr->CallN ||
                      BuiltinCalls::invariant(CallP(expr)->getCallId()))) {
      return HOISTABLE;
    }
    auto ap = args.begin();
    for (auto a = ListP(expr)->head; a; a = a->nextArg, ++ap) {
      if (shouldHoist(*ap)) {
        hoist(&a->value);
      }
    }
    break;
  }
  case AST::BinaryN: {
    auto b = (Binary *)expr;
    switch (b->op) {
    case Binary::NEG:
    case Binary::SET_GLOBAL:
    case Binary::NOT: {
      return checkHoist(&b->right) + 1;
    }
    case Binary::LOOKUP: {
      // can we ever hoist a look up operation?
      hoistInvariants(&b->right);
      break;
    }
    case Binary::CONCAT: {
      return checkHoistConcat(exprHome);
    }
    case Binary::MATCH: {
      // this operation side-effects the dynamic variables
      // so can be hoisted only if they are not used
      hoistInvariants(&b->left);
      hoistInvariants(&b->right);
      break;
    }
    case Binary::REPLACE: {
      // treat replace/match pair as a ternary-op
      auto m = b->left->isOp(b->MATCH);
      assert(m && "expected MATCH operand to replace");
      auto hml = checkHoist(&m->left);
      auto hmr = checkHoist(&m->right);
      auto hr = checkHoist(&b->right);
      if (::isInvariant(hml + hmr + hr)) {
        return HoistInfo(true, HOISTABLE_COST);
      }
      if (shouldHoist(hml)) {
        hoist(&m->left);
      }
      if (shouldHoist(hmr)) {
        hoist(&m->right);
      }
      if (shouldHoist(hr)) {
        hoist(&b->right);
      }
      break;
    }
    case Binary::SUBSCRIPT:
    case Binary::EQ:
    case Binary::NE:
    case Binary::LT:
    case Binary::LE:
    case Binary::GE:
    case Binary::GT:
    case Binary::ADD:
    case Binary::SUB:
    case Binary::MUL:
    case Binary::DIV:
    case Binary::AND:
    case Binary::OR: {
      auto hl = checkHoist(&b->left);
      auto hr = checkHoist(&b->right);
      if (::isInvariant(hl + hr)) {
        return HoistInfo(true, HOISTABLE_COST);
      }
      if (shouldHoist(hl)) {
        hoist(&b->left);
      }
      if (shouldHoist(hr)) {
        hoist(&b->right);
      }
      break;
    }
    }
    break;
  }
  }
  return HoistInfo(false, 0);
}

void Optimizer::hoist(Expression **expr) {
  auto e = *expr;
  if (RSED_Debug::debug) {
    std::cout << "hoiosting: ";
    e->dump();
    std::cout << "\n";
  }
  auto h = new HoistedValue(e);
  *expr = new HoistedValueRef(e);
  if (!firstInvariant) {
    firstInvariant = h;
  } else {
    lastInvariant->setNext(h);
  }
  lastInvariant = h;
  return;
}
