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
using std::string;
int debug;
extern int yydebug;
static int dump = 0;
static bool scriptIn;
string input("");

DEFINE_bool(debug, false, "enable debugging");
DEFINE_bool(yydebug, false, "enable parser debugging");
DEFINE_bool(dump, false, "dump parsed script");
DEFINE_string(input, "", "input file to be processed");
DEFINE_bool(script_in, false, "read script from stdin");
string script;

void parseOptions(int argc, char *argv[]) {

  gflags::ParseCommandLineFlags(&(argc), &(argv), true);

  // only command line arguments are preserved by gflags, so the 0th one is
  // the tool name and 1st is the input.
  debug = FLAGS_debug;
  yydebug = FLAGS_yydebug;
  dump = FLAGS_dump;
  input = FLAGS_input;
  scriptIn = FLAGS_script_in;
  if (scriptIn) {
    if (input == "") {
      std::cerr << argv[0] << ": missing input file\n";
      exit(1);
    }
    script = "";
  } else if (argc < 2) {
    std::cerr << argv[0] << ": missing script parameter\n";
    exit(1);
  }
  else {
    script = argv[1];
  }
}

int main(int argc, char *argv[]) {

  parseOptions(argc, argv);
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
