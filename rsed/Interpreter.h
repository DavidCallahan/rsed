//
//  Interpreter.h
//  rsed
//
//  Created by David Callahan on 6/27/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#ifndef __rsed__Interpreter__
#define __rsed__Interpreter__

class State;

class Interpreter {
  State * state = nullptr;
public:
  Interpreter() {  }
  void initialize();
  bool setInput(const std::string & fileName);
  void interpret(class Statement *);
  int getReturnCode() const ;
};


#endif /* defined(__rsed__Interpreter__) */
