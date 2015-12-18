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

class Statement;
class LineBuffer;

class Exception {
public:
  const Statement *statement = nullptr;
  LineBuffer *input = nullptr;
  std::string message;
  Exception(std::string message) : message(message){};
  Exception(std::string message, const Statement *statement)
      : statement(statement), message(message) {}
  Exception(std::string message, const Statement *statement, LineBuffer * input)
  : statement(statement), input(input), message(message) {}
  void setStatement(Statement *s) {
    if (!statement)
      statement = s;
  }
};

#endif /* Exceptions_h */
