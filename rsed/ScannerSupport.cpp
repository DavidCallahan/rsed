//
//  Scanner.cpp
//  rsed
//
//  Created by David Callahan on 12/18/15.
//
//

#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <regex>
#include "AST.h"
#include "Scanner.h"
using std::string;


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

static std::regex terminator("^\\s*[\"']?([[a-zA-Z_][a-zA-Z0-9_]*)([\"']([rxig]*))?\\s*$");

// implement shell-style multilin strings
// we jsut saw "<<", now find teh terminator string
string *Scanner::multilineString(string* unputText) {

  std::smatch match;
  string line;
  char c;
  while ( (c = yyinput()) != '\n' ) {
    if (c == 0)
      goto eof;
    line.append(1,c);
  }
  if (!std::regex_match(line, match, terminator)) {
    error() << "invalid multiline terminator\n";
    return new string("");
  }
  {
    std::stringstream comment;
    string line2;
    string term = match[1].str();
    comment << '"';
    bool first = true;
    for (;;) {
      line2.clear();
      while ((c = yyinput()) != '\n') {
        if (c == 0)
          goto eof;
        line2.append(1,c);
      }
      if (line2.length() >= term.length()) {
        auto res =std::mismatch(term.begin(), term.end(), line2.begin()) ;
        if (res.first == term.end()) {
          unputText->append(res.second, line2.end());
          unputText->append(1, '\n');
          break;
        }
      }
      if(!first) { comment << '\n' ; }
      first= false;
      comment << line2;
    }
    comment << '"' << match[3];
    return new string(comment.str());

  }
eof:
  error() << "end of file found scanning multiline string\n";
  return new string("\"\"");
}