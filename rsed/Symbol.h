//
//  Symbol.h
//  rsed
//
//  Created by David Callahan on 6/27/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#ifndef __rsed__Symbol__
#define __rsed__Symbol__
#include <string>

class Symbol {
  std::string name;

public:
  Symbol(const std::string &name) : name(name) {}
  const std::string &getName() { return name; }

  virtual const std::string getValue() const = 0;
  virtual void setValue(const std::string &v) = 0;
  virtual ~Symbol() {}

  static Symbol *findSymbol(const std::string &name);
  static void defineSymbol(Symbol *sym);
};

class SimpleSymbol : public Symbol {
  std::string value;

public:
  SimpleSymbol(const std::string &name) : Symbol(name), value("") {}
  const std::string getValue() const override { return value; }
  void setValue(const std::string &v) override { value = v; }
  ~SimpleSymbol() {}
};

template <typename Action> class DynamicSymbol : public Symbol {
  const Action action;

public:
  DynamicSymbol(const std::string &name, const Action &action)
      : Symbol(name), action(std::move(action)) {}
  const std::string getValue() const override { return action(); }
  void setValue(const std::string &v) override {}
  ~DynamicSymbol() {}
};
template <typename Action>
inline DynamicSymbol<Action> *makeSymbol(const std::string &name,
                                         const Action &action) {
  return new DynamicSymbol<Action>(name, action);
}

#endif /* defined(__rsed__Symbol__) */
