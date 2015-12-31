//
//  BuiltinCalls.hpp
//  rsed
//
//  Created by David Callahan on 12/12/15.
//  Copyright © 2015 David Callahan. All rights reserved.
//

#ifndef BuiltinCalls_hpp
#define BuiltinCalls_hpp
#include <string>
#include <vector>
#include "StringRef.h"
#include <Value.h>
class EvalState;
namespace BuiltinCalls {
enum Builtins {
  TRIM = 0,
  SHELL,
  LENGTH,
  JOIN,
  ESCAPE,
  MKTEMP,
  EXPAND,
  SUBSTR,
  QUOTE,
  IFNULL,
  LOGICAL,
  NUMBERB,
  STRINGB,
  APPEND,
};
bool getCallId(const std::string &name, unsigned *);
void evalCall(unsigned id, std::vector<Value *> &args, EvalState *,
              Value *result);
Value::Kind callKind(unsigned id);
bool invariant(unsigned id);
}

#endif /* BuiltinCalls_hpp */
