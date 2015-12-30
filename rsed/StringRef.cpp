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
    if (c == '\\' && (i + 1) < n) {
      switch (s[++i]) {
      case 'n':
        c = '\n';
        break;
      case 't':
        c = '\t';
        break;
      case 'v':
        c = '\v';
        break;
      case 'b':
        c = '\b';
        break;
      case 'r':
        c = '\r';
        break;
      case 'f':
        c = '\f';
        break;
      case 'a':
        c = '\a';
        break;
      case '\\':
        c = '\\';
        break;
      case '?':
        c = '\?';
        break;
      case '\'':
        c = '\'';
        break;
      case '"':
        c = '\"';
        break;
      case '0':
        c = '\0';
        break;
      default:
        i -= 1;
      }
    }
    result.append(1, c);
  }
  return result;
}
}

StringRef::StringRef(std::string *text) : flags(0) {
  auto len = text->length();
  auto delim = text->at(0);
  for (; len > 2; len--) {
    auto c = text->at(len - 1);
    if (c == delim) {
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
    case 'x':
      flags |= ESCAPE_SPECIALS;
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
    if (s.getFlags() &  s.ESCAPE_SPECIALS) {
      OS << 'x';
    }
  }
  return OS;
}
