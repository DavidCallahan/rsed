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
#include "Value.h"
#include "Exception.h"

class Symbol : protected Value {
  std::string name;

public:
  Symbol(const std::string &name) : name(name) {}
  const std::string &getName() { return name; }

  virtual void setValue(const std::string &v) = 0;
  virtual ~Symbol() {}
  virtual bool isDynamic() const { return false; }
  virtual void set(Value *) {
    throw Exception("unable to modify symbol " + name);
  }
  virtual Value * getValue() { return this; }
  static Symbol *findSymbol(const std::string &name);
  static Symbol *newTempSymbol();
  static void defineSymbol(Symbol *sym);
};

class SimpleSymbol : public Symbol  {

public:
  SimpleSymbol(const std::string &name, std::string value = "")
  : Symbol(name) {  Value::set(value); }
  void setValue(const std::string &v) override { Value::set(v); }
  void set(Value * value) override { Value::set(value); }
  ~SimpleSymbol() {}
};

template <typename Action> class DynamicSymbol : public Symbol {
  const Action action;

public:
  DynamicSymbol(const std::string &name, const Action &action)
      : Symbol(name), action(std::move(action)) {}
  Value * getValue() override {
    Value::set(action());
    return this;
  }
  void setValue(const std::string &v) override {}
  virtual bool isDynamic() const override { return true; }
  ~DynamicSymbol() {}
};
template <typename Action>
inline DynamicSymbol<Action> *makeSymbol(const std::string &name,
                                         const Action &action) {
  return new DynamicSymbol<Action>(name, action);
}

#endif /* defined(__rsed__Symbol__) */
