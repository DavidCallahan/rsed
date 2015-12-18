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
#include <cstdio>
#include <memory>
#include <stdlib.h>
#include "EvalState.h"
#include "LineBuffer.h"
#include "RegEx.h"

namespace {
enum Builtins {
  TRIM = 0,
  REPLACE,
  REPLACE_ALL,
  SHELL,
  LENGTH,
  JOIN,
  ESCAPE,
  MKTEMP,
  EXPAND,
  QUOTE
};
typedef std::pair<unsigned, string> BuiltinName;
vector<BuiltinName> builtins{{LENGTH, "length"},
                             {TRIM, "trim"},
                             {REPLACE, "substitue"},
                             {REPLACE, "sub"},
                             {REPLACE_ALL, "suball"},
                             {JOIN, "join"},
                             {ESCAPE, "escape"},
                             {QUOTE, "quote"},
                             {MKTEMP, "mktemp"},
                             {EXPAND, "expand"},
                             {SHELL, "shell"}};

string doQuote(char quote, const string &text) {
  string result;
  result.append(1, quote);
  for (auto c : text) {
    if (c == quote) {
      result.append(1, '\\');
    }
    result.append(1, c);
  }
  result.append(1, quote);
  return result;
}
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
  bool replaceAll = false;
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
  case REPLACE_ALL:
    replaceAll = true;
  /*FALLTHROUGH*/
  case REPLACE: {
    if (args.size() < 3) {
      state->error() << "Two few arguments to replace()\n";
      return "";
    }
    auto regEx = state->getRegEx();
    int r;
    if (replaceAll) {
      auto p = args[0];
      p.setIsGlobal();
      r = regEx->setPattern(p);
    } else {
      r = regEx->setPattern(args[0]);
    }
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
    break;
  }
  case ESCAPE:
    for (auto &str : args) {
      ss << state->getRegEx()->escape(str.getText());
    }
    break;

  case QUOTE: {
    char quote = '"';
    auto ap = args.begin();
    if (args.size() > 1) {
      auto &q = *ap++;
      if (q.getText().empty()) {
        state->error() << "empty quote specification in quote()\n";
        return "";
      }
      quote = q.getText()[0];
    }
    auto end = args.end();
    for (; ap != end; ++ap) {
      ss << doQuote(quote, ap->getText());
    }
    break;
  }
  case MKTEMP: {
    static char buffer[] = "rsedXXXXXX";
    auto t = ::mktemp(buffer);
    LineBuffer::tempFileNames.emplace_back(t);
    ss << t;
    break;
  }
  case EXPAND:
    for (auto a : args) {
      ss << state->expandVariables(a.getText());
    }
    break;
  }
  return ss.str();
}
}
