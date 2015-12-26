//
//  ExpandVariables.hpp
//  rsed
//
//  Created by David Callahan on 12/26/15.
//
//

#ifndef ExpandVariables_hpp
#define ExpandVariables_hpp
#include <sstream>
#include <StringRef.h>

class ExpandVariables {
public:
  virtual void single(const std::string & text, unsigned flags=0) = 0;
  virtual void string(std::stringstream & s, unsigned flags=0) =0;
  virtual void varMatch(unsigned i) =0;
  virtual void variable(std::string name) =0;
  void expand(const StringRef &);
};

#endif /* ExpandVariables_hpp */
