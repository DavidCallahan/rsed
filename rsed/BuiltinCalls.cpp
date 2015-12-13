//
//  BuiltinCalls.cpp
//  rsed
//
//  Created by David Callahan on 12/12/15.
//  Copyright © 2015 David Callahan. All rights reserved.
//

#include "BuiltinCalls.hpp"

bool getCallId(const std::string &name, unsigned *u) {
  *u = 0;
  if (name == "trim") {
    return true;
  }
  return false;
}
