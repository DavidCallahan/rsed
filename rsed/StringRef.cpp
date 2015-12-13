//
//  StringRef.cpp
//  rsed
//
//  Created by David Callahan on 6/29/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//
#include <iostream>
#include "StringRef.h"

namespace {
std::string asCLiteral(const std::string &s) {
  std::string result = "";
  auto n = s.length();
  for (size_t i = 0; i < n; i++) {
    auto c = s[i];
    if (c == '\\' && (i+1) < n) {
      switch(s[++i]) {
        case 'n':  c = '\n' ; break ;
        case 't':  c = '\t' ; break ;
        case 'v':  c = '\v' ; break ;
        case 'b':  c = '\b' ; break ;
        case 'r':  c = '\r' ; break ;
        case 'f':  c = '\f' ; break ;
        case 'a':  c = '\a' ; break ;
        case '\\': c = '\\' ; break ;
        case '?':  c = '\?' ; break ;
        case '\'': c = '\'' ; break ;
        case '"':  c = '\"' ; break ;
        case '0':  c = '\0' ; break ;
        default:
          i -= 1;
      }
    }
    result.append(1,c);
  }
  return result;
}
}

StringRef::StringRef(std::string *text) : flags(0) {
  auto len = text->length();
  for (; len > 2; len--) {
    auto c = text->at(len - 1);
    if (c == '"') {
      break;
    }
    switch (c) {
    case 'i':
      flags |= CASE_INSENSITIVE;
      break;
    case 'r':
      flags |= RAW_STRING;
      break;
    case 'g':
      flags |= GLOBAL;
      break;
    }
  }
  this->text = text->substr(1, len - 2);
  if (!(flags & RAW_STRING)) {
    this->text = asCLiteral(this->text);
  }
  delete text;
}
std::ostream &operator<<(std::ostream &OS, const StringRef &s) {
  OS << '\"' << s.getText() << '"';
  if (s.getFlags()) {
    if (s.getFlags() & s.RAW_STRING) {
      OS << 'r';
    }
    if (s.getFlags() & s.CASE_INSENSITIVE) {
      OS << 'i';
    }
    if (s.getFlags() & s.GLOBAL) {
      OS << 'g';
    }
  }
  return OS;
}

// for non-raw strings, interpret C-style escape sequences
StringRef StringRef::processEscapes(const StringRef &r) {
  StringRef s;
  s.flags = r.flags;
  if (r.flags & r.RAW_STRING) {
    s.text = r.text;
  } else {
    for (unsigned i = 0; i < r.text.length(); i++) {
      auto c = r.text[i];
      if (c != '\\') {
        s.text.append(1, c);
      } else {
        c = r.text[++i];
        switch (c) {
        case '\\':
          s.text.append(1, '\\');
          break;
        case 'A':
        case 'a':
          s.text.append(1, '\a');
          break;
        case 'B':
        case 'b':
          s.text.append(1, '\b');
          break;
        case 'F':
        case 'f':
          s.text.append(1, '\f');
          break;
        case 'n':
        case 'N':
          s.text.append(1, '\n');
          break;
        case 'r':
        case 'R':
          s.text.append(1, '\r');
          break;
        case 't':
        case 'T':
          s.text.append(1, '\t');
          break;
        case 'v':
        case 'V':
          s.text.append(1, '\v');
          break;
        default:
          s.text.append(1, '\\');
          s.text.append(1, c);
        }
      }
    }
  }
  return s;
}
