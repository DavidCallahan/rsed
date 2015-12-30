//
//  RegEx.cpp
//  rsed
//
//  Created by David Callahan on 6/28/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include <assert.h>
#include <iostream>
#include <regex>
#include <string>
#include <cstring>
#include <exception>
#include "StringRef.h"
#include "RegEx.h"
#include "Exception.h"

using namespace std::regex_constants;
using std::vector;
using std::string;
namespace RSED_Debug {
extern int debug;
}

namespace {

class C14RegEx : public RegEx {
  static syntax_option_type regExOptions;
  static bool specials[];
  std::vector<std::pair<std::regex, unsigned>> patterns;
  std::smatch matches;
  std::string lastTarget;

public:
  virtual int setStyle(const std::string &style) override;
  virtual void setPattern(const StringRef &pattern, int index) override;

  virtual bool match(int pattern, const std::string &line) override;
  virtual void split(int pattern, const std::string &target,
                     std::vector<std::string> *words) override;
  virtual std::string escape(const std::string &text) override;
  virtual std::string getSubMatch(unsigned i) override;
  virtual std::string replace(int pattern, const std::string &replacement,
                              const std::string &line) override;
};

syntax_option_type C14RegEx::regExOptions = basic;

std::regex createRegex(const string &str) {
  try {
    std::regex temp(str);
    return temp;
  } catch (std::exception &) {
    throw Exception("invalid regular expression: " + str);
  }
}
}

bool C14RegEx::specials[256];
RegEx *RegEx::regEx = nullptr;
std::string RegEx::styleName;

bool C14RegEx::match(int pattern, const std::string &line) {
  lastTarget = line;
  return std::regex_search(lastTarget, matches, patterns[pattern].first);
}

std::string C14RegEx::getSubMatch(unsigned int i) {
  if (i >= matches.size()) {
    return "";
  }
  return matches[i];
}

void C14RegEx::setPattern(const StringRef &pattern, int index) {
  if (index >= patterns.size()) {
    patterns.resize(2 * index + 1);
  }
  auto regex = createRegex(pattern.getText());
  patterns[index] = std::make_pair(std::move(regex) ,pattern.getFlags());
}

std::string C14RegEx::escape(const std::string &text) {
  std::string result;
  for (unsigned c : text) {
    if (specials[c]) {
      result.append(1, '\\');
    }
    result.append(1, c);
  }
  return std::move(result);
}

void RegEx::setDefaultRegEx() {
  regEx = new C14RegEx;
  regEx->setStyle("basic");
}

// maybe thos should not be in C14RegEx but rather used
// to select which implementation to use
// Needed for: ^ $ \ . * + ? ( ) [ ] { }
int C14RegEx::setStyle(const std::string &style) {
  const char *list = R"(^$\.*+?()[]{})";
  memset(specials, false, sizeof(specials));
  while (*list) {
    specials[(int)*list] = true;
    list += 1;
  }

  if (style == "ECMAScript") {
    regExOptions = ECMAScript;
  } else if (style == "basic") {
    regExOptions = basic;
  } else if (style == "extended") {
    regExOptions = extended;
  } else if (style == "grep") {
    regExOptions = grep;
  } else if (style == "egrep") {
    regExOptions = egrep;
  } else if (style == "awk") {
    regExOptions = awk;
  } else {
    return 1;
  }
  styleName = style;
  return 0;
}

std::string C14RegEx::replace(int pattern, const std::string &replacement,
                              const std::string &line) {
  std::regex &re = patterns[pattern].first;
  unsigned flags = patterns[pattern].second;
  if (flags & StringRef::GLOBAL) {
    return std::regex_replace(line, re, replacement);
  }
  return std::regex_replace(line, re, replacement, format_first_only);
}

void C14RegEx::split(int pattern, const std::string &target,
                     std::vector<std::string> *words) {
  assert(pattern < patterns.size());
  std::sregex_token_iterator iter(target.begin(), target.end(),
                                  patterns[pattern].first, -1);
  std::sregex_token_iterator end;
  for (; iter != end; ++iter) {
    std::string p = *iter;
    words->emplace_back(std::move(p));
  }
}
