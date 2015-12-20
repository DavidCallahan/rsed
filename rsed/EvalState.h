//
//  EvalState.h
//  rsed
//
//  Created by David Callahan on 12/13/15.
//
//

#ifndef EvalState_h
#define EvalState_h
#include <iostream>
#include <string>
class Expression;
class RegEx;

class EvalState {
protected:
  RegEx *regEx = nullptr;
public:
  virtual unsigned getLineno() const = 0;
  RegEx *getRegEx() const { return regEx; }
  void setRegEx(RegEx *regEx) { this->regEx = regEx; }
  virtual std::string expandVariables(const std::string &) = 0;
};

#endif /* EvalState_h */
