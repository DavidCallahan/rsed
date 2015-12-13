//
//  BuiltinCalls.cpp
//  rsed
//
//  Created by David Callahan on 12/12/15.
//  Copyright © 2015 David Callahan. All rights reserved.
//

#include "BuiltinCalls.h"
#include "EvalState.h"
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

string evalCall(unsigned int id, const vector<StringRef> &args, EvalState * state) {
  std::stringstream ss;
  if (id == TRIM) {
    const std::string &delimiters = " \f\n\r\t\v";
    for (auto & sr : args) {
      string s = sr.getText();
      s.erase(s.find_last_not_of(delimiters) + 1);
      s.erase(0, s.find_first_not_of(delimiters));
      ss << s;
    }
  }
  else if (id == REPLACE) {
    if (args.size() < 3) {
      state->error() << "Two few arguments to replace()\n" ;
      return "";
    }
    auto regEx = state->getRegEx();
    int r = regEx->setPattern(args[0]);
    if (r < 0) {
      state->error() << "Error parsing regular expression: " << args[0] << '\n';
      return "";
    }
    auto & replaceText = args[1].getText();
    auto last = args.end();
    for (auto p = args.begin() +2; p != last; ++p) {
      auto rs = regEx->replace(r, replaceText, p->getText());
      ss << rs;
    }
    regEx->releasePattern(r);
  }
  return ss.str();
}
}
