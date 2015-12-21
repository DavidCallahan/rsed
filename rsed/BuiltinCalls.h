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
class Value;
class EvalState;
namespace BuiltinCalls {
bool getCallId(const std::string &name, unsigned *);
std::string evalCall(unsigned id, std::vector<Value *> &args, EvalState *);
}

#endif /* BuiltinCalls_hpp */
