//
//  Parser.cpp
//  rsed
//
//  Created by David Callahan on 6/25/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include "AST.h"
#include "Scanner.h"
#include "Parser.h"

int yyparse(Statement **parseTree, Scanner *scanner);
extern int yydebug;

Statement *Parser::parse(const std::string &script) {
  // yydebug = 1;
  Scanner s;
  if (!s.init(script.c_str())) {
    return nullptr;
  }
  Statement *result = nullptr;
  auto rv = yyparse(&result, &s);
  return (rv || s.sawError? nullptr : result);
}