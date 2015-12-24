#include <string>
#ifndef yyFlexLexer
#include <FlexLexer.h>
#endif
#include <iostream>

class Scanner : public yyFlexLexer {
public:
  Scanner() {}
  int init(const char *source);
  int yylex();
  bool sawError = false;
  std::ostream &error() {
    sawError = true;
    return std::cerr << lineno() << "; ";
  }
  
  std::string * multilineString();
};

class StmtList {
  Statement * head, *tail;
public:
  StmtList & append(Statement * s) {
    if(head) { tail->setNext(s); }
    else     head = s;
    tail = s;
    return *this;
  }
  Statement * getHead() const { return head; }
  static StmtList emptyList() {
    StmtList temp;
    temp.head = temp.tail = nullptr;
    return temp;
  }
} ;
