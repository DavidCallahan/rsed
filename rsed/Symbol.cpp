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
#include <sstream>
#include <string>

using std::string;
namespace {
std::unordered_map<string, Symbol *> stringMap;
unsigned nextTemp = 0;
}

Symbol *Symbol::findSymbol(const string &name) {
  auto p = stringMap.insert(std::make_pair(name, nullptr));
  if (p.second) {
    auto s = new SimpleSymbol(std::move(name));
    if (auto e = getenv(name.c_str())) {
      s->setValue(e);
    }
    p.first->second = s;
  }
  return p.first->second;
}

void Symbol::defineSymbol(Symbol *sym) {
  auto &s = stringMap[sym->name];
  if (s) {
    delete s;
  }
  s = sym;
}

Symbol *Symbol::newTempSymbol() {
  for(;;) {
    std::stringstream buffer;
    buffer << "temp" << nextTemp++;
    string name = buffer.str();
    auto p = stringMap.insert(std::make_pair(name, nullptr));
    if (p.second) {
      auto s = new SimpleSymbol(std::move(name));
      p.first->second = s;
      return s;
    }
  }
}