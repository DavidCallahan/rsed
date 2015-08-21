//
//  Buffer.cpp
//  rsed
//
//  Created by David Callahan on 6/27/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include "LineBuffer.h"
#include <unordered_map>

namespace {
  std::unordered_map<std::string, LineBuffer *> bufferIn, bufferOut;
}

LineBuffer *LineBuffer::findOutputBuffer(const std::string & name) {
  assert(!"not yet implemented"); // TODO
}