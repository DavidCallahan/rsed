//
//  Value.cpp
//  rsed
//
//  Created by David Callahan on 12/18/15.
//
//

#include "Value.h"
#include <sstream>
#include <assert.h>
#include "Exception.h"
using std::string;
using std::stringstream;

void Value::set(const Value *value) {
  kind = value->kind;
  scache.clear();
  cur = nullptr;
  switch (kind) {
  case Logical:
    logical = value->logical;
    break;
  case Number:
    number = value->number;
    break;
  case RegEx:
    regEx = value->regEx;
    break;
  case String:
    if (auto s = value->constString()) {
      cur = std::move(s);
    } else {
      cur = nullptr;
      scache = value->scache;
    }
    break;
  case List:
    list.clear();
    for (auto &v : value->list) {
      list.push_back(v);
    }
    break;
  }
}

bool Value::asLogical() const {
  switch (kind) {
  case Logical:
    return logical;
  case Number:
    return number != 0 && number == number;
  case String: {
    auto s = getString();
    return !(s.getText().empty() || s.getText() == "false");
  }
  case List:
    return !list.empty();
  case RegEx:
    assert(0 && "can not convert regex to logical");
    throw;
  }
}

double Value::asNumber() {
  switch (kind) {
  case Logical:
    return double(logical);
  case Number:
    return number;
  case List:
    asString();
  // fallthorugh
  case String: {
    stringstream ss(getString().getText());
    double result;
    ss >> result;
    if (ss.fail()) {
      throw Exception("unable to convert to number: " + getString().getText());
    }
    return result;
  }
  case RegEx:
    assert(0 && "can not convert regex to number");
    throw;
  }
}

static const StringRef trueString("true", 0);
static const StringRef falseString("false", 0);

const StringRef &Value::asString() {

  if (cur) {
    return *cur;
  }
  if (!scache.getText().empty()) {
    return scache;
  }

  switch (kind) {
  case Logical: {
    set(asShared(logical ? trueString : falseString));
    break;
  }
  case Number: {
    stringstream ss;
    if (number != number) {
      ss << "NaN";
    } else {
      ss << number;
    }
    set(ss.str());
    break;
  }
  case List: {
    stringstream ss;
    for (auto &v : list) {
      ss << v.asString().getText();
    }
    set(ss.str()); // TODO propagate flags?
    break;
  }
  case String:
    return scache;
  case RegEx:
    assert(0 && "can not convert regex to number");
  }
  return getString();
}

StringPtr Value::asStringPtr() {
  if (!cur) {
    cur = std::make_shared<const StringRef>(asString());
  }
  return cur;
}

std::ostream &operator<<(std::ostream &OS, const Value &value) {
  switch (value.kind) {
  case Value::RegEx:
    OS << "regex[" << value.regEx << "]";
    break;
  case Value::List: {
    char sep = '[';
    for (auto &v : value.list) {
      OS << sep << v;
      sep = ',';
    }
    OS << ']';
    break;
  }
  case Value::Logical:
    OS << (value.logical ? "true" : "false");
    break;
  case Value::Number:
    OS << value.number;
    break;
  case Value::String:
    OS << value.getString();
    break;
  }
  return OS;
}

void Value::set(bool boole) {
  logical = boole;
  kind = Logical;
  scache.clear();
  cur = nullptr;
}

void Value::set(double n) {
  kind = Number;
  number = n;
  scache.clear();
  cur = nullptr;
}

void Value::set(StringRef s) {
  kind = String;
  scache = std::move(s);
  cur = nullptr;
}
void Value::set(StringPtr s) {
  kind = String;
  scache.clear();
  cur = std::move(s);
}

void Value::setRegEx(unsigned int i) {
  kind = RegEx;
  regEx = i;
  scache.clear();
  cur = nullptr;
}

void Value::set(std::string s) { set(StringRef(std::move(s))); }

unsigned Value::getRegEx() {
  assert(kind == RegEx);
  return regEx;
}

template <typename T> static int cmp(const T &left, const T &right) {
  if (left < right)
    return -1;
  if (left == right)
    return 0;
  return 1;
}

int compare(Value *left, Value *right) {
  auto k = std::min(left->kind, right->kind);
  if (k == left->List) {
    k = left->String;
  }
  switch (k) {
  case Value::Logical:
    return cmp(left->asLogical(), right->asLogical());
  case Value::Number:
    return cmp(left->asNumber(), right->asNumber());
  case Value::String:
    return cmp(left->asString().getText(), right->asString().getText());
  case Value::List:
  case Value::RegEx:
    assert(0 && "can not compare regex");
    throw;
  }
}

void Value::append(const Value &v) {
  assert(kind == List);
  list.emplace_back(v);
}