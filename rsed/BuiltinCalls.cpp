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
enum Builtins { TRIM = 0, REPLACE };
typedef std::pair<unsigned, string> BuiltinName;
vector<BuiltinName> builtins{
    {TRIM, "trim"}, {REPLACE, "replace"},
};
}

namespace BuiltinCalls {

bool getCallId(const string &name, unsigned *u) {
  for (auto &s : builtins) {
    if (name == s.second) {
      *u = s.first;
      return true;
    }
  }
  return false;
}

string evalCall(unsigned int id, const vector<string> &args) {
  if (id == TRIM) {
    const std::string &delimiters = " \f\n\r\t\v";
    std::stringstream ss;
    for (auto s : args) {
      s.erase(s.find_last_not_of(delimiters) + 1);
      s.erase(0, s.find_first_not_of(delimiters));
      ss << s;
    }
    return ss.str();
  }
  if (id == REPLACE) {
    
  }
  return "";
}
}
