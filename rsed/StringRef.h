//
//  StringRef.h
//  rsed
//
//  Created by David Callahan on 6/28/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#ifndef rsed_StringRef_h
#define rsed_StringRef_h
#include <string>
#include <sstream>

class StringRef {
  std::string text;
  unsigned flags;

public:
  enum {
    RAW_STRING = 1,
    CASE_INSENSITIVE = 2,
    GLOBAL = 4,
  };

  StringRef() : flags(0) {}
  StringRef(std::string text, unsigned flags) : text(text), flags(flags) {}
  unsigned getFlags() const { return flags; }
  const std::string &getText() const { return text; }
  StringRef(std::string *asScanned);

  void append(const std::string &r) { text.append(r); }
  void append(const StringRef &r) { append(r.text); }
  void append(int n) {
    std::stringstream ss;
    ss << n;
    std::string temp;
    ss >> temp;
    append(temp);
  }

  bool isRaw() const { return flags & RAW_STRING; }
  void setIsRaw() { flags |= RAW_STRING; }
  void setIsGlobal() { flags |= GLOBAL; }

  static StringRef processEscapes(const StringRef &r);
};
std::ostream &operator<<(std::ostream &, const StringRef &);

#endif
