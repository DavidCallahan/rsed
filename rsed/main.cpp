//
//  main.cpp
//  rsed
//
//  Created by David Callahan on 6/24/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include <iostream>
#include <assert.h>
#include "AST.h"
#include "Parser.h"
#include "Scanner.h"
#include "RegEx.h"
#include "Interpreter.h"
#include "gflags/gflags.h"
#include "Optimize.h"

int debug;
extern int yydebug;
static int dump = 0;
std::string input("");

DEFINE_bool(debug, false, "enable debugging");
DEFINE_bool(yydebug, false, "enable parser debugging");
DEFINE_bool(dump, false, "dump parsed script");
DEFINE_string(input, "", "input file to be processed");

std::string parseOptions(int argc, char *argv[]) {

  gflags::ParseCommandLineFlags(&(argc), &(argv), true);

  // only command line arguments are preserved by gflags, so the 0th one is
  // the tool name and 1st is the input.
  assert(argc == 2 && "Input is required");
  debug = FLAGS_debug;
  yydebug = FLAGS_yydebug;
  dump = FLAGS_dump;
  input = FLAGS_input;
  return argv[1];
}

int main(int argc, char *argv[]) {

  std::string script = parseOptions(argc, argv);
  if (argc == 1) {
    std::cerr << "usage: rsed <script>\n";
    exit(1);
  }
  RegEx::setDefaultRegEx();
  Interpreter interp;
  interp.initialize();

  Parser parser;
  Statement *ast = parser.parse(script);
  if (!ast) {
    exit(1);
  }
  if (dump) {
    ast->dump();
  }
  ast = Optimize::optimize(ast);
  if (input != "") {
    bool ok = interp.setInput(input);
    if (!ok) {
      exit(1);
    }
  }
  interp.interpret(ast);
  exit(interp.getReturnCode());
}

void breakPoint() { std::cout << "at break\n"; }
