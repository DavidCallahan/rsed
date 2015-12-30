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
    sref = value->sref;
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
  case String:
    return !(sref.getText().empty() || sref.getText() == "false");
  case List:
    return !list.empty();
  case RegEx:
    assert(0 && "can not convert regex to logical");
  }
}

double Value::asNumber() {
  switch (kind) {
  case Logical:
    return double(logical);
  case Number:
    return number;
  case String: {
    stringstream ss(sref.getText());
    double result;
    ss >> result;
    if (ss.fail()) {
      throw Exception("unable to convert to number: " + sref);
    }
    return result;
  }
  case List: {
  }
  case RegEx:
    assert(0 && "can not convert regex to number");
  }
}
const StringRef &Value::asString() {

  if (!sref.getText().empty()) {
    return sref;
  }

  switch (kind) {
  case Logical:
    sref = StringRef(logical ? "true" : "false", 0);
    break;
  case Number: {
    stringstream ss;
    if (number != number) {
      ss << "NaN";
    } else {
      ss << number;
    }
    sref = StringRef(ss.str(), 0);
    break;
  }
  case List: {
    stringstream ss;
    for (auto &v : list) {
      ss << v.asString().getText();
    }
    sref = StringRef(ss.str(), 0); // TODO propagate flags?
    break;
  }
  case String:
    break;
  case RegEx:
    assert(0 && "can not convert regex to number");
  }
  return sref;
}

std::ostream &operator<<(std::ostream &OS, Value &value) {
  if (value.kind == value.RegEx) {
    OS << "regex[" << value.getRegEx() << "]";
  } else if (value.kind == value.List) {
    char sep = '[';
    for (auto &v : value.list) {
      OS << sep << v;
      sep = ',';
    }
    OS << ']';
  } else {
    OS << value.asString();
  }
  return OS;
}

void Value::set(bool boole) {
  logical = boole;
  kind = Logical;
  sref.clear();
}

void Value::set(double n) {
  kind = Number;
  number = n;
  sref.clear();
}

void Value::set(StringRef s) {
  kind = String;
  sref = s;
}

void Value::setRegEx(unsigned int i) {
  kind = RegEx;
  regEx = i;
}

void Value::set(std::string s) { set(StringRef(s)); }

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
  }
}
