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
public:
  enum Kind {
    String,
    Logical,
    Number,
    RegEx,
  };
  Kind kind;
  StringRef sref;
  bool logical = false;
  double number = 0.0;
  unsigned regEx;
  Value(StringRef string) : kind(String), sref{string} {}
  Value(bool logical = false) : kind(Logical), logical(logical) {}
  Value(double number) : kind(Number), number(number) {}
  
  bool isString() const { return kind == String; }
  const StringRef &asString();
  bool asLogical() const;
  double asNumber();
  unsigned getRegEx() ;

  void set(bool);
  void set(double);
  void set(StringRef);
  void set(std::string);
  void set(Value * value) {
    kind = value->kind;
    logical = value->logical;
    number = value->number;
    regEx = value->regEx;
    sref = value->sref;
  }
  void setRegEx(unsigned i);
  void append(ValueP &);
};
std::ostream &operator<<(std::ostream &OS, const Value &value);
int compare(Value * left, Value * right);


#endif /* Value_hpp */
