//
//  ASTWalk.h
//  rsed
//
//  Created by David Callahan on 12/17/15.
//
//

#ifndef ASTWalk_h
#define ASTWalk_h
#include "AST.h"

template <typename ACTION> AST::WalkResult Statement::walk(const ACTION &a) {
  Statement *s = this;
  for (; s; s = s->getNext()) {
    auto rc = a(s);
    if (rc == StopW)
      return rc;
    if (rc == SkipChildrenW)
      continue;

    switch (s->kind()) {
    case ForeachN: {
      rc = ForeachP(s)->body->walk(a);
      if (rc == StopW)
        return rc;
      break;
    }

    case IfStmtN: {
      auto ifs = (IfStatement *)s;
      rc = ifs->thenStmts->walk(a);
      if (rc == StopW)
        return rc;
      if (rc == ContinueW) {
        if (auto elseStmts = ifs->elseStmts) {
          rc = elseStmts->walk(a);
          if (rc == StopW)
            return rc;
        }
      }
    }
    default:
      break;
    }
  }
  return ContinueW;
}

template <typename ACTION>
AST::WalkResult Statement::walkExprs(const ACTION &a) {
  auto rc = ContinueW;
  switch (kind()) {
  case InputN:
  case CloseN:
  case OutputN:
    return ((IOStmt *)this)->buffer->walkDown(a);
  case HoistedValueN:
    return ((HoistedValue *)this)->rhs->walkDown(a);
  case ForeachN:
    return ((Foreach *)this)->control->walkDown(a);
  case PrintN:
    rc = ((Print *)this)->text->walkDown(a);
    if (rc == ContinueW) {
      if (auto buffer = ((Print *)this)->buffer) {
        rc = buffer->walkDown(a);
      }
    }
    break;
  case CopyN:
  case SkipN:
    break;
  case ReplaceN:
    rc = ((Replace *)this)->pattern->walkDown(a);
    if (rc != ContinueW)
      return rc;
    rc = ((Replace *)this)->replacement->walkDown(a);
    break;
  case SplitN:
  case SplitInputN:
    if (auto sep = ((Split *)this)->separator) {
      rc = sep->walkDown(a);
      if (rc != ContinueW)
        return rc;
    }
    rc = ((Split *)this)->separator->walkDown(a);
    break;
  case SetN:
  case SetAppendN:
    rc = ((Set *)this)->rhs->walkDown(a);
    break;
  case IfStmtN:
    rc = ((IfStatement *)this)->predicate->walkDown(a);
    break;
  case ErrorN:
    rc = ((Error *)this)->text->walkDown(a);
    break;
  case ColumnsN:
  case ColumnsInputN:
    rc = ((Columns *)this)->columns->walkDown(a);
    break;
  case RequiredN: {
    auto r = (Required *)this;
    if (auto p = r->predicate) {
      rc = p->walkDown(a);
    }
    if (rc == ContinueW && r->errMsg) {
      rc = r->errMsg->walkDown(a);
    }
    break;
  }
  }
  return rc;
}

template <typename ACTION> void Statement::applyExprs(const ACTION &a) {
  switch (kind()) {
  case ForeachN:
    a(((Foreach *)this)->control);
    break;
  case InputN:
  case CloseN:
  case OutputN:
    a(((IOStmt *)this)->buffer);
    break;
  case PrintN:
    a(((Print *)this)->text);
    a(((Print *)this)->buffer);
    break;
  case CopyN:
  case SkipN:
    break;
  case ReplaceN:
    a(((Replace *)this)->pattern);
    a(((Replace *)this)->replacement);
    break;
  case SplitN:
  case SplitInputN:
    a(((Split *)this)->separator);
    a(((Split *)this)->target);
    break;
  case SetN:
  case SetAppendN:
    a(((Set *)this)->rhs);
    break;
  case IfStmtN: {
    auto ifs = (IfStatement *)this;
    a(ifs->predicate);
    ifs->thenStmts->applyExprs(a);
    if (ifs->elseStmts) {
      ifs->elseStmts->applyExprs(a);
    }
    break;
  }
  case ErrorN:
    a(((Error *)this)->text);
    break;
  case ColumnsN:
  case ColumnsInputN:
    a(((Columns *)this)->columns);
    break;
  case RequiredN:
    a(((Required *)this)->predicate);
    if (((Required *)this)->errMsg) {
      a(((Required *)this)->errMsg);
    }
    break;
  case HoistedValueN:
    a(((HoistedValue *)this)->rhs);
    break;
  }
  if (getNext()) {
    getNext()->applyExprs(a);
  }
}

template <typename ACTION> AST::WalkResult Expression::walkUp(const ACTION &a) {
  Expression *e = this;
  WalkResult rc = ContinueW;
  switch (e->kind()) {
  case ControlN: {
    auto c = (Control *)e;
    rc = c->pattern->walkUp(a);
    if (rc == ContinueW && c->errorMsg) {
      rc = c->errorMsg->walkUp(a);
    }
    break;
  }
  case BinaryN:
    if (BinaryP(e)->left) {
      rc = walkUp(BinaryP(e)->left);
      if (rc != ContinueW)
        return rc;
    }
    rc = walkUp(BinaryP(e)->right);
    break;
  case CallN:
  case ListN:
    for (auto arg = CallP(e)->head; arg; arg = arg->nextArg) {
      rc = arg->value->walkUp(a);
      if (rc != ContinueW)
        return rc;
    }
    break;
  case RegExPatternN:
    rc = ((RegExPattern *)e)->pattern->walkUp(a);
    break;
  case VariableN:
  case NumberN:
  case LogicalN:
  case VarMatchN:
  case StringConstN:
  case ArgN:
  case HoistedValueRefN:
    break;
  }
  if (rc != ContinueW)
    return rc;
  rc = a(e);
  if (rc != ContinueW)
    return rc;
  return ContinueW;
}

template <typename ACTION>
AST::WalkResult Expression::walkDown(const ACTION &a) {
  Expression *e = this;
  WalkResult rc = a(e);
  if (rc != ContinueW) {
    if (rc == SkipChildrenW)
      rc = ContinueW;
    return rc;
  }

  switch (e->kind()) {
  case ControlN: {
    auto c = (Control *)e;
    rc = c->pattern->walkDown(a);
    if (rc != ContinueW)
      return rc;
    if (c->errorMsg) {
      rc = c->errorMsg->walkDown(a);
    }
    break;
  }
  case BinaryN:
    if (auto left = BinaryP(e)->left) {
      rc = left->walkDown(a);
      if (rc != ContinueW)
        return rc;
    }
    rc = BinaryP(e)->right->walkDown(a);
    if (rc != ContinueW)
      return rc;
    break;
  case CallN:
  case ListN:
    for (auto arg = ListP(e)->head; arg; arg = arg->nextArg) {
      rc = arg->value->walkDown(a);
      if (rc != ContinueW)
        return rc;
    }
    break;
  case RegExPatternN:
    rc = ((RegExPattern *)e)->pattern->walkDown(a);
    break;
  case HoistedValueRefN:
  case ArgN:
  case NumberN:
  case LogicalN:
  case StringConstN:
  case VarMatchN:
  case VariableN:
    break;
  }
  return ContinueW;
}

template <typename ACTION>
void Expression::walkConcat(const ACTION &a, std::vector<Binary *> *operators) {
  if (auto c = isOp(CONCAT)) {
    if (operators) {
      operators->push_back(c);
    }
    c->left->walkConcat(a, operators);
    c->right->walkConcat(a, operators);
  } else if (kind() == HoistedValueRefN) {
    ((HoistedValueRef *)this)->value->walkConcat(a, operators);
  } else {
    a(this);
  }
}

#endif /* ASTWalk_h */
