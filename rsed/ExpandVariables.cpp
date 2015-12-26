//
//  ExpandVariables.cpp
//  rsed
//
//  Created by David Callahan on 12/26/15.
//
//

#include "ExpandVariables.h"
#include <regex>
#include <vector>
#include <sstream>
using std::vector;
using std::stringstream;

static std::regex variable(R"((\\\$)|(\$[a-zA-Z][a-zA-Z0-9_]+)|(\$\{[^}]+\})|(\$[0-9]+))");

void ExpandVariables::expand(const StringRef & text) {
  
  if (text.isRaw()) {
    single(text.getText(),text.getFlags());
    return ;
  }
  const char *ctext = text.getText().c_str();
  size_t len = text.getText().length();
  auto vars_begin = std::cregex_iterator(ctext, ctext + len, ::variable);
  auto vars_end = std::cregex_iterator();
  if (vars_begin == vars_end) {
    single(text.getText(), text.getFlags());
    return ;
  }
  
  stringstream str;
  int strLength = 0;
  unsigned numStringCounts = 0;
  const char *last = ctext;
  for (std::cregex_iterator vars = vars_begin; vars != vars_end; ++vars) {
    auto match = (*vars)[0];
    const char *cvar = match.first;
    str.write(last, cvar - last);
    strLength += (cvar - last);
    if (*cvar == '\\') {
      str << '$';
      strLength += 1;
    } else {
      if (strLength > 0) {
        string(str, text.getFlags());
        numStringCounts += 1;
        str.str("");
        strLength = 0;
      }
      auto vstart = cvar +1 ;
      auto vlen   = match.second - vstart;
      if (isdigit(*vstart)) {
        stringstream temp;
        temp.write(vstart, vlen);
        unsigned i;
        temp >> i;
        varMatch(i);
      } else {
        if (*vstart == '{') {
          vstart += 1;
          vlen -= 2;
        }
        variable(std::string(vstart, vlen));
      }
    }
    last = match.second;
  }
  auto clast = ctext + len;
  if (last != clast) {
    str.write(last, clast - last);
    strLength += clast - last;
  }
  if (strLength > 0 || numStringCounts == 0) {
    string(str, text.getFlags());
  }
}
