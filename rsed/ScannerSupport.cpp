//
//  Scanner.cpp
//  rsed
//
//  Created by David Callahan on 12/18/15.
//
//

#include <string>
#include <fstream>
#include <sstream>
#include <regex>
#include "AST.h"
#include "Scanner.h"

int Scanner::init(const char *source) {
  if (*source) {
    auto in = new std::ifstream;
    in->open(source);
    yyin = in;
    return in->is_open();
  }
  yyin = &std::cin;
  return 1;
}

static std::regex terminator("^\\s*[\"']?([^\"']+)([\"'](r)?)?\\s*$");

// implement shell-style multilin strings
// we jsut saw "<<", now find teh terminator string
std::string *Scanner::multilineString() {

  std::smatch match;
  std::string line;
  char c;
  while ( (c = yyinput()) != '\n' ) {
    if (c == EOF)
      goto eof;
    line.append(1,c);
  }
  if (!std::regex_match(line, match, terminator)) {
    error() << "invalid multiline terminator\n";
    return new std::string("");
  }
  {
    std::stringstream comment;
    std::string line2;
    comment << '"';
    bool first = true;
    for (;;) {
      line2.clear();
      while ((c = yyinput()) != '\n') {
        if (c == EOF)
          goto eof;
        line2.append(1,c);
      }
      if (line2 == match[1]) {
        break;
      }
      if(!first) { comment << '\n' ; }
      first= false;
      comment << line2;
    }
    comment << '"' << match[3];
    return new std::string(comment.str());

  }
eof:
  error() << "end of file found scanning multiline string\n";
  return new std::string("");
}