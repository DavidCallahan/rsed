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
#include "Exception.h"
#include "Value.h"

namespace {
enum Builtins {
  TRIM = 0,
  SHELL,
  LENGTH,
  JOIN,
  ESCAPE,
  MKTEMP,
  EXPAND,
  SUBSTR,
  QUOTE
};
typedef std::pair<unsigned, string> BuiltinName;
vector<BuiltinName> builtins{{LENGTH, "length"},
                             {TRIM, "trim"},
                             {JOIN, "join"},
                             {ESCAPE, "escape"},
                             {QUOTE, "quote"},
                             {MKTEMP, "mktemp"},
                             {EXPAND, "expand"},
                             {SUBSTR, "substr"},
                             {SUBSTR, "substring"},
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

namespace BuiltinCalls {

Value::Kind callKind(unsigned id) {
  return  (id == LENGTH ? Value::Number : Value::String);
}

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

void evalCall(unsigned int id, vector<Value *> &args, EvalState *state,
              Value *result) {
  std::stringstream ss;
  switch (Builtins(id)) {
  case TRIM: {
    const std::string &delimiters = " \f\n\r\t\v";
    for (auto &sr : args) {
      string s = sr->asString().getText();
      s.erase(s.find_last_not_of(delimiters) + 1);
      s.erase(0, s.find_first_not_of(delimiters));
      ss << s;
    }
    break;
  }
  case LENGTH: {
    result->set(double(
        args.size() > 0 ? args.front()->asString().getText().length() : 0));
    return;
  }
  case JOIN: {
    if (args.size() < 2) {
      break;
    }
    auto sep = args[0]->asString().getText();
    auto output = args[1]->asString().getText();
    auto size = output.length();
    ss << output;

    for (auto i = 2; i < args.size(); i++) {
      if (size > 0)
        ss << sep;
      ss << args[i]->asString().getText();
      size += args[i]->asString().getText().length();
    }
    break;
  }
  case ESCAPE:
    for (auto &str : args) {
      ss << state->getRegEx()->escape(str->asString().getText());
    }
    break;

  case QUOTE: {
    char quote = '"';
    auto ap = args.begin();
    if (args.size() > 1) {
      auto &q = *ap++;
      if (q->asString().getText().empty()) {
        throw Exception("empty quote specification in quote()");
      }
      quote = q->asString().getText()[0];
    }
    auto end = args.end();
    for (; ap != end; ++ap) {
      ss << doQuote(quote, (*ap)->asString().getText());
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
      state->expandVariables(a->asString().getText(), ss);
    }
    break;
  case SHELL:
    throw Exception("shell function not yet implemented:");
  case SUBSTR: {
    auto n = args.size();
    if (n == 0)
      break;
    if (n == 1) {
      result->set(args[0]->asString());
      return;
    }
    const auto &text = args[0]->asString().getText();
    auto start = (int)args[1]->asNumber();
    if (start < 0) {
      start = std::max(0, int(text.length() + start));
    }
    if (n > 2) {
      auto len = (int)args[2]->asNumber();
      if (len < 0) {
        throw Exception("negative length value for substring");
      }
      result->set(text.substr(start, len));
      return;
    }
    result->set(text.substr(start));
    return;
  }
  }
  result->set(ss.str());
}
}
