//
//  Exception.h
//  rsed
//
//  Created by David Callahan on 12/18/15.
//
//

#ifndef Exception_h
#define Exception_h
#include <string>
#include <memory>
#include <iostream>

class Statement;
class LineBuffer;

class Exception {
public:
  const Statement *statement = nullptr;
  std::shared_ptr<LineBuffer> input = nullptr;
  std::string message;
  Exception(std::string message) : message(message){};
  Exception(std::string message, const Statement *statement)
      : statement(statement), message(message) {}
  Exception(std::string message, const Statement *statement,
            std::shared_ptr<LineBuffer> input)
      : statement(statement), input(input), message(message) {}
  void setStatement(Statement *s, std::shared_ptr<LineBuffer> i) {
    if (!statement)
      statement = s;
    if (!input)
      input = i;
  }
};

#endif /* Exceptions_h */
