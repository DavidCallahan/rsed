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
#include <cstdio>
#include <memory>

namespace {
enum Builtins { TRIM = 0, REPLACE, SHELL, LENGTH, JOIN };
typedef std::pair<unsigned, string> BuiltinName;
vector<BuiltinName> builtins{{LENGTH, "length"},
                             {TRIM, "trim"},
                             {REPLACE, "substitue"},
                             {REPLACE, "sub"},
                             {JOIN, "join"},
                             {SHELL, "shell"}};
}
// TODO -- length, substring
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

#if 0
bool shell(string cmd, vector<string> * lines) {
  std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe) return false;
  char buffer[128];
  std::string result = "";
  std::ifstream input(pipe);
  
  while (!feof(pipe.get())) {
    if (fgets(buffer, 128, pipe.get()) != NULL)
      result += buffer;
  }
  return result;
}
#endif

string evalCall(unsigned int id, const vector<StringRef> &args,
                EvalState *state) {
  std::stringstream ss;
  switch (id) {
  case TRIM: {
    const std::string &delimiters = " \f\n\r\t\v";
    for (auto &sr : args) {
      string s = sr.getText();
      s.erase(s.find_last_not_of(delimiters) + 1);
      s.erase(0, s.find_first_not_of(delimiters));
      ss << s;
    }
    break;
  }
  case REPLACE: {
    if (args.size() < 3) {
      state->error() << "Two few arguments to replace()\n";
      return "";
    }
    auto regEx = state->getRegEx();
    int r = regEx->setPattern(args[0]);
    if (r < 0) {
      state->error() << "Error parsing regular expression: " << args[0] << '\n';
      return "";
    }
    auto &replaceText = args[1].getText();
    auto last = args.end();
    for (auto p = args.begin() + 2; p != last; ++p) {
      auto rs = regEx->replace(r, replaceText, p->getText());
      ss << rs;
    }
    regEx->releasePattern(r);
    break;
  }
  case LENGTH: {
    ss << (args.size() > 0 ? args.front().getText().length() : 0);
    break;
  }
  case JOIN: {
    if (args.size() < 2) {
      break;
    }
    auto sep = args[0].getText();
    auto output = args[1].getText();
    auto size = output.length();
    ss << output;

    for (auto i = 2; i < args.size(); i++) {
      if (size > 0)
        ss << sep;
      ss << args[i].getText();
      size += args[i].getText().length();
    }
  }
  }
  return ss.str();
}
}
