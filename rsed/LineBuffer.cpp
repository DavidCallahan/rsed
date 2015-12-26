//
//  Buffer.cpp
//  rsed
//
//  Created by David Callahan on 6/27/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include "LineBuffer.h"
#include "Exception.h"
#include <string>
#include <unordered_map>
using std::string;
using std::ifstream;
using std::ofstream;
using std::shared_ptr;

namespace {
struct Buffer {
  std::shared_ptr<LineBuffer> input = nullptr;
  std::shared_ptr<LineBuffer> output = nullptr;
};
std::unordered_map<std::string, Buffer> buffers;
}

std::vector<string> LineBuffer::tempFileNames;
LineBufferCloser::~LineBufferCloser() {
  for (auto & name : LineBuffer::tempFileNames) {
    auto p = buffers.find(name);
    if (p != buffers.end()) {
      Buffer & b = p->second;
      if (b.input) b.input->close();
      if (b.output) b.output->close();
      std::remove(name.c_str());
    }
  }
}

std::shared_ptr<LineBuffer> LineBuffer::findOutputBuffer(const std::string &name) {
  auto p = buffers.insert(std::make_pair(name, Buffer()));
  Buffer &b = p.first->second;
  if (b.input) {
    b.input->close();
    b.input = nullptr;
  }
  if (!b.output) {
    auto f = new ofstream(name);
    if (!f->is_open()) {
      string error("unable to open file: ") ;
      error += name;
      throw error;
    }
    b.output.reset(new StreamOutBuffer<ofstream>(f,name));
  }
  return b.output;
}

std::shared_ptr<LineBuffer> LineBuffer::findInputBuffer(const std::string &name) {
  auto p = buffers.insert(std::make_pair(name, Buffer()));
  Buffer &b = p.first->second;
  if (b.output) {
    b.output->close();
    b.output = nullptr;
  }
  if (!b.input) {
    auto f = new ifstream(name);
    if (!f->is_open()) {
      string error ="unable to open file: " ;
      error += name;
      throw Exception(error);
    }
    b.input.reset(new StreamInBuffer<ifstream>(f,name));
  }
  return b.input;
}

std::shared_ptr<LineBuffer> LineBuffer::closeBuffer(const std::string & name) {
  auto p = buffers.find(name);
  if (p == buffers.end()) {
    return nullptr;
  }
  Buffer &b = p->second;
  if(b.output) {
    b.output->close();
    auto rval = b.output;
    b.output = nullptr;
    assert(!b.input);
    return rval;
  }
  if (b.input) {
    b.input->close();
    auto rval = b.input;
    b.input = nullptr;
    return rval;
  }
  return nullptr;
}
