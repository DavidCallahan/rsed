//
//  Value.hpp
//  rsed
//
//  Created by David Callahan on 12/18/15.
//
//

#ifndef Value_hpp
#define Value_hpp
#include "StringRef.h"

#include <vector>
#include <iostream>

// typedef std::unique_ptr<class Value> ValueP;
typedef class Value *ValueP;

class Value {
  StringRef scache;
public:
  enum Kind {
    String,
    Logical,
    Number,
    RegEx,
    List,
  };
  Kind kind;
  StringPtr cur = nullptr;
  bool logical = false;
  double number = 0.0;
  unsigned regEx;
  std::vector<Value> list;
  Value(StringRef string) : scache{string}, kind(String)  {}
  Value(StringPtr string) : kind(String), cur(string) {}
  Value(bool logical = false) : kind(Logical), logical(logical) {}
  Value(double number) : kind(Number), number(number) {}
  Value(const Value &value) { set(&value); }
  
  const StringRef & getString() const {
    return (cur ? *cur : scache);
  }
  bool isString() const { return kind == String; }
  const StringRef &asString();
  bool asLogical() const;
  double asNumber();
  unsigned getRegEx();
  StringPtr asStringPtr();
  StringPtr constString() const { return cur; }

  void set(bool);
  void set(double);
  void set(StringRef);
  void set(StringPtr);
  void set(std::string);
  void set(const Value *value);
  void setString(Value * v) {
    if (auto s = v->constString()) {
      set(s);
    }
    else {
      set(v->asString());
    }
  }
  void setRegEx(unsigned i);
  void append(const Value &);
  void clearList() {
    scache.clear();
    cur = nullptr;
    list.clear();
    kind = List;
  }
  void listAppend(Value *v) {
    if (v->kind == List) {
      for (auto &lv : v->list) {
        list.emplace_back(lv);
      }
    } else {
      list.emplace_back(*v);
    }
  }
};
std::ostream &operator<<(std::ostream &OS, const Value &value);
int compare(Value *left, Value *right);

#endif /* Value_hpp */
