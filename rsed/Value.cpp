//
//  Value.cpp
//  rsed
//
//  Created by David Callahan on 12/18/15.
//
//

#include "Value.h"
#include <sstream>
#include "Exception.h"
using std::string;
using std::stringstream;

bool Value::asLogical() const {
  switch (kind) {
  case Logical:
    return logical;
  case Number:
    return number != 0;
  case String:
    return !(sref.getText().empty() || sref.getText() != "false");
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
    ss << number;
    sref = StringRef(ss.str(), 0);
  }
  case String:
    break;
  }
  return sref;
}


std::ostream &operator<<(std::ostream &OS, Value &value) {
  OS << value.asString();
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

void Value::set(std::string s) { set(StringRef(s)); }
