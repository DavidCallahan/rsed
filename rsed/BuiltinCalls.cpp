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
                             {APPEND, "append"},
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
  switch (id) {
  case LENGTH:
  case NUMBERB:
    return Value::Number;
  case LOGICAL:
    return Value::Logical;
  default:
    return Value::String;
  }
}

bool invariant(unsigned id) {
  switch (id) {
  case EXPAND:
  case SHELL:
  case MKTEMP:
    return false;
  default:
    return true;
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

StringPtr shell(vector<Value *> &args) {
  std::stringstream ss;
  for (auto v : args) {
    ss << v->asString().getText() << " ";
  }
  std::string shellCmd = ss.str();
  auto pipe = LineBuffer::makePipeBuffer(shellCmd);
  pipe->nextLine();
  auto result = pipe->getInputLine();
  pipe->close();
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
    bool first = true;
    auto append = [&first, &ss, sep](Value *v) {
      if (!first) {
        ss << sep;
      } else
        first = false;
      ss << v->asString().getText();
    };
    for (auto i = 1; i < args.size(); i++) {
      if (args[i]->kind == Value::List) {
        for (auto &v : args[i]->list) {
          append(&v);
        }
      } else {
        append(args[i]);
      }
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
    result->set(shell(args));
    return;
  case SUBSTR: {
    auto n = args.size();
    if (n == 0)
      break;
    if (n == 1) {
      result->setString(args[0]);
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
    if (args.size() == 1) {
      result->setString(args[0]);
      return;
    }
    for (auto v : args) {
      ss << v->asString().getText();
    }
    break;
  }
  case APPEND: {
    // TODO does APPEND flatten? or only sort-of flatten?
    //   if it changes, also change printListElt in interpret.cpp
    result->clearList();
    for (auto v : args) {
      result->listAppend(v);
    }
    return;
  }
  }
  result->set(ss.str());
}
}
