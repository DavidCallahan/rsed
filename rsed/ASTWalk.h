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
    case ControlN:
      return ((Control *)this)->pattern->walkDown(a);
    case InputN:
      return ((Input *)this)->buffer->walkDown(a);
    case OutputN:
      return ((Output *)this)->buffer->walkDown(a);
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
      if ((Split *)this->pattern) {
        rc = ((Split *)this)->separator->walkDown(a);
        if (rc != ContinueW)
          return rc;
      }
      rc = ((Split *)this)->separator->walkDown(a);
      break;
    case SetN:
      rc = ((Set *)this)->rhs->walkDown(a);
      break;
    case IfStmtN:
      rc = ((IfStatement *)this)->predicate->walkDown(a);
      break;
    case ErrorN:
      rc = ((Error *)this)->text->walkDown(a);
      break;
    case ColumnsN:
      rc = ((Columns *)this)->columns->walkDown(a);
      break;
  }
  
  return rc;
}

template <typename ACTION> AST::WalkResult Expression::walkUp(const ACTION &a) {
  Expression *e = this;
  WalkResult rc = ContinueW;
  switch (e->kind()) {
    case ControlN:
      rc = ((Control *)e)->pattern->walkUp(a);
      break;
    case BinaryN:
      if (BinaryP(e)->left) {
        rc = walkUp(BinaryP(e)->left);
        if (rc != ContinueW)
          return rc;
      }
      rc = walkUp(BinaryP(e)->right);
      break;
    case CallN:
      for (auto arg = CallP(e)->args; arg; arg = arg->nextArg) {
        rc = arg->value->walkUp(a);
        if (rc != ContinueW)
          return rc;
      }
      break;
    default:
      rc = ContinueW;
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
    case ControlN:
      rc = ((Control *)e)->pattern->walkDown(a);
      if (rc != ContinueW)
        return rc;
      break;
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
      for (auto arg = CallP(e)->args; arg; arg = arg->nextArg) {
        rc = arg->value->walkDown(a);
        if (rc != ContinueW)
          return rc;
      }
      break;
      
    default:
      break;
  }
  return ContinueW;
}


#endif /* ASTWalk_h */
