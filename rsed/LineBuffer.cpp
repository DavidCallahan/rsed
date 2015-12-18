//
//  Buffer.cpp
//  rsed
//
//  Created by David Callahan on 6/27/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include "LineBuffer.h"
#include <unordered_map>
#include <string>
using std::string;
using std::ifstream;
using std::ofstream;
namespace {
struct Buffer {
  LineBuffer *input = nullptr;
  LineBuffer *output = nullptr;
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

LineBuffer *LineBuffer::findOutputBuffer(const std::string &name) {
  auto p = buffers.insert(std::make_pair(name, Buffer()));
  Buffer &b = p.first->second;
  if (b.input) {
    b.input->close();
    delete b.input;
    b.input = nullptr;
  }
  if (!b.output) {
    auto f = new ofstream(name);
    if (!f->is_open()) {
      string error("unable to open file: ") ;
      error += name;
      throw error;
    }
    b.output = new StreamOutBuffer<ofstream>(f);
  }
  return b.output;
}

LineBuffer *LineBuffer::findInputBuffer(const std::string &name) {
  auto p = buffers.insert(std::make_pair(name, Buffer()));
  Buffer &b = p.first->second;
  if (b.output) {
    b.output->close();
    delete b.output;
    b.output = nullptr;
  }
  if (!b.input) {
    auto f = new ifstream(name);
    if (!f->is_open()) {
      string error("unable to open file: ") ;
      error += name;
      throw error;
    }
    b.input = new StreamInBuffer<ifstream>(f);
  }
  return b.output;
}