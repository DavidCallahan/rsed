//
//  main.cpp
//  rsed
//
//  Created by David Callahan on 6/24/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include <iostream>
#include <assert.h>
#include "gflags/gflags.h"
#include "rsed.h"
#include "AST.h"
#include "Parser.h"
#include "Scanner.h"
#include "RegEx.h"
#include "Exception.h"
#include "Interpreter.h"
#include "Optimize.h"
#include "LineBuffer.h"

using std::string;

namespace RSED {
  int debug = 0;
  int dump = 0;
  std::ofstream env_save;
}
using namespace RSED;
extern int yydebug;
static bool scriptIn;
string input("");

DEFINE_bool(debug, false, "enable debugging");
DEFINE_bool(yydebug, false, "enable parser debugging");
DEFINE_bool(dump, false, "dump parsed script");
DEFINE_string(input, "", "input file to be processed");
DEFINE_bool(script_in, false, "read script from stdin");
DEFINE_string(env_save, "", "file to save referenced environment variables");
string script;


void parseOptions(int *argc, char **argv[]) {

  gflags::ParseCommandLineFlags(argc, argv, true);

  // only command line arguments are preserved by gflags, so the 0th one is
  // the tool name and 1st is the input.
  debug = FLAGS_debug;
  yydebug = FLAGS_yydebug;
  dump = FLAGS_dump;
  input = FLAGS_input;
  scriptIn = FLAGS_script_in;
  const char * err = nullptr;
  if (scriptIn) {
    if (input == "") {
      err = "missing input file";
    }
    script = "";
  } else if (*argc < 2) {
    err = "missing script parameter";
  } else {
    script = (*argv)[1];
    *argc -= 1;
    *argv += 1;
  }
  if (FLAGS_env_save != "") {
    env_save.open(FLAGS_env_save);
    if (!env_save.is_open()) {
        err = "unable to open env_save file";
    }
  }
  if (err) {
    std::cerr << argv[0] << ":" << err << '\n';
    exit (1);
  }
}

std::ostream &operator<<(std::ostream &OS, const Exception &e) {
  if (e.input && e.input->getLineno() > 0) {
    OS << "input " << e.input->getLineno() << ": ";
  }
  if (e.statement) {
    OS << "script " << e.statement->getSourceLine() << ": ";
  }
  return OS << e.message << '\n';
}


int main(int argc, char *argv[]) {

  parseOptions(&argc, &argv);
  RegEx::setDefaultRegEx();
  Interpreter interp;
  try {
    interp.initialize(argc, argv);
  }
  catch (Exception &e) {
    std::cerr << e;
    exit(1);
  }

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

  int rc = 0;
  try {
    interp.interpret(ast);
    LineBuffer::closeAll();
  }
  catch (Exception & e) {
    std::cerr << e;
    rc = 1;
  }
  exit(rc);
}

void breakPoint() { std::cout << "at break\n"; }
