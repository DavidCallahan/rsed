//
//  Parser.h
//  rsed
//
//  Created by David Callahan on 6/25/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#ifndef rsed_Parser_h
#define rsed_Parser_h

class Parser {
public:
  class Statement * parse(const std::string &);
};

#endif
