//
//  RegEx.h
//  rsed
//
//  Created by David Callahan on 6/28/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#ifndef __rsed__RegEx__
#define __rsed__RegEx__
#include <string>
#include <regex>
#include "StringRef.h"

class RegEx {
protected:
  static std::string styleName;

public:
  // set once and govens the entire script
  virtual int setStyle(const std::string &style) = 0;

  // compile a pattern which is then referred to by index
  virtual void setPattern(StringPtr, int index) = 0;

  // perform a replacement operation
  virtual std::string replace(int pattern, const std::string &replacement,
                              const std::string &line) = 0;
  virtual bool match(int pattern, const std::string &line) = 0;
  virtual void split(int pattern,
                     const std::string & target,
                     std::vector<std::string> * words) = 0;

  virtual std::string escape(const std::string &text) = 0;

  // return a group by nnumber
  virtual std::string getSubMatch(unsigned i) = 0;

  static RegEx *regEx;
  static void setDefaultRegEx();
  static const std::string &getStyleName() { return styleName; }
};

#endif /* defined(__rsed__RegEx__) */
