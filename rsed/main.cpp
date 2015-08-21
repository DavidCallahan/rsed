//
//  main.cpp
//  rsed
//
//  Created by David Callahan on 6/24/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include <iostream>
#include "cxxopts.hpp"
#include "AST.h"
#include "Parser.h"
#include "Scanner.h"
#include "RegEx.h"
#include "Interpreter.h"

int debug;
extern int yydebug ;
static int dump = 0;
std::string input("");

std::string parseOptions(int argc, char *argv[]) {

  cxxopts::Options options(argv[0], "readable sed");
  options.add_options()
    ("y,yydebug", "enable parser debugging")
    ("d,dump", "dump ast")
    ("h,help", "print usage information")
    ("f,file", "input file", cxxopts::value<std::string>())
    ("input", "script file", cxxopts::value<std::string>())
    ;
  options.parse_positional("input");
  options.parse(argc, argv);
  if (options.count("help") || !options.count("input")) {
    std::cout << options.help() << std::endl;
    exit(0);
  }
  if (options.count("yydebug")) {
    yydebug = 1;
  }
  if(options.count("file")) {
    input = options["file"].as<std::string>();
  }
  dump = options.count("dump");
  return options["input"].as<std::string>();
}

int main(int argc, char *argv[]) {

  std::string script = parseOptions(argc, argv);
  if(argc == 1) {
    std::cerr << "usage: rsed <script>\n";
    exit(1);
  }
  RegEx::setDefaultRegEx();
  Parser parser;
  Statement * ast = parser.parse(script);
  if(!ast) {
    exit(1);
  }
  if(dump) {
    ast->dump();
  }
  Interpreter interp;
  interp.initialize();
  if(input != "") {
    bool ok = interp.setInput(input);
    if(! ok) {
      exit(1);
    }
  }
  interp.interpret(ast);
  exit(interp.getReturnCode());
}
