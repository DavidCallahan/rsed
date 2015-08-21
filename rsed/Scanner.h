#include <string>
#ifndef yyFlexLexer
#include <FlexLexer.h>
#endif
class Scanner : public yyFlexLexer {
public:
  Scanner() {} 
  int init(const char * source);
  int yylex();
  std::string * getString();
};
