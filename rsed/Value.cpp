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

bool Value::asLogical() const {
  switch (kind) {
  case Logical:
    return logical;
  case Number:
    return number != 0 && number==number;
  case String:
    return !(sref.getText().empty() || sref.getText() == "false");
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
    }
    else {
      ss << number;
    }
    sref = StringRef(ss.str(), 0);
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
  switch (std::min(left->kind, right->kind)) {
  case Value::Logical:
    return cmp(left->asLogical(), right->asLogical());
  case Value::Number:
    return cmp(left->asNumber(), right->asNumber());
  case Value::String:
    return cmp(left->asString().getText(), right->asString().getText());
  case Value::RegEx:
    assert(0 && "can not compare regex");
  }
}
