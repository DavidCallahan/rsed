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
};
