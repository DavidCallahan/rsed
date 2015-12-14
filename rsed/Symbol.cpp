//
//  Symbol.cpp
//  rsed
//
//  Created by David Callahan on 6/27/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include "Symbol.h"
#include <unordered_map>
#include <stdlib.h>
using std::string;
namespace {
std::unordered_map<string, Symbol *> stringMap;
}

Symbol *Symbol::findSymbol(const string &name) {
  auto p = stringMap.find(name);
  if (p != stringMap.end()) {
    return p->second;
  }
  auto &s = stringMap[name];
  s = new SimpleSymbol(std::move(name));
  if (auto e = getenv(name.c_str())) {
    s->setValue(e);
  }
  return s;
}

void Symbol::defineSymbol(Symbol *sym) {
  auto &s = stringMap[sym->name];
  if (s) {
    delete s;
  }
  s = sym;
}
