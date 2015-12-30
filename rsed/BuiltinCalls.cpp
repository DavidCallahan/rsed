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
#include "Symbol.h"

namespace BuiltinCalls {

namespace {
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
                             {IFNULL, "ifnull"},
                             {LOGICAL, "logical"},
                             {NUMBERB, "number"},
                             {STRINGB, "string"},
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

Value::Kind callKind(unsigned id) {
  switch(id) {
    case LENGTH:
    case NUMBERB:
      return Value::Number;
    case LOGICAL:
      return Value::Logical;
    default:
      return Value::String;
  }
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

string shell(vector<Value *> &args) {
  std::stringstream ss;
  for (auto v : args) {
    ss << v->asString().getText() << " ";
  }
  std::string shellCmd = ss.str();
  std::shared_ptr<FILE> pipe;
  try {
    pipe.reset(popen(shellCmd.c_str(), "r"), pclose);
  } catch (...) {
    throw Exception("error executing command: " + shellCmd);
  }
  if (!pipe) {
    throw Exception("error executing command: " + shellCmd);
  }
  std::string result = "";
  while (!feof(pipe.get())) {
    char c = fgetc(pipe.get());
    if (c == '\n' || c == EOF) {
      break;
    }
    result.append(1, c);
  }
  auto status = pclose(pipe.get());
  if (status != 0) {
    throw Exception("error executing command: " + shellCmd);
  }
  return result;
}

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
    ss << shell(args);
    break;
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
  case IFNULL: {
    if (args.size() < 2) {
      throw Exception("at least two args required for ifnull()");
    }
    for (auto v : args) {
      auto s = v->asString().getText();
      if (s != "") {
        ss << s;
        break;
      }
    }
    break;
  }
  case LOGICAL: {
    if (args.size() < 1) {
      throw Exception("at least one arg required for logical()");
    }
    result->set(args[0]->asLogical());
    return;
  }
  case NUMBERB: {
    if (args.size() < 1) {
      throw Exception("at least one arg required for number()");
    }
    result->set(args[0]->asNumber());
    return;
  }
  case STRINGB: {
    for (auto v : args) {
      ss << v->asString().getText();
    }
  }
  }
  result->set(ss.str());
}
}
