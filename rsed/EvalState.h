//
//  EvalState.h
//  rsed
//
//  Created by David Callahan on 12/13/15.
//
//

#ifndef EvalState_h
#define EvalState_h
#include "RegEx.h"
#include <iostream>

class EvalState {
protected:
  bool sawError = false;
  RegEx *regEx = nullptr;

public:
  std::ostream &error() {
    sawError = true;
    return std::cerr << getLineno() << ": ";
  }
  virtual unsigned getLineno() const = 0;
  RegEx *getRegEx() const { return regEx; }
  void setRegEx(RegEx *regEx) { this->regEx = regEx; }
};

#endif /* EvalState_h */
