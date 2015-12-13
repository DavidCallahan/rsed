//
//  BuiltinCalls.cpp
//  rsed
//
//  Created by David Callahan on 12/12/15.
//  Copyright © 2015 David Callahan. All rights reserved.
//

#include "BuiltinCalls.h"
using std::string;
#include <vector>
using std::vector;
#include <sstream>
#include <iostream>

namespace {
vector<string> builtins{"trim", "replace"};
}

namespace BuiltinCalls {

bool getCallId(const string &name, unsigned *u) {
  unsigned i = 0;
  for (auto &s : builtins) {
    if (name == s) {
      *u = i;
      return true;
    }
    ++i;
  }
  return false;
}

string evalCall(unsigned int id, const vector<string> &args) {
  if (id == 0) {
    const std::string &delimiters = " \f\n\r\t\v";
    std::stringstream ss;
    for (auto s : args) {
      s.erase(s.find_last_not_of(delimiters) + 1);
      s.erase(0, s.find_first_not_of(delimiters));
      ss << s;
    }
    return ss.str();
    
  }
  return "";

}
}
