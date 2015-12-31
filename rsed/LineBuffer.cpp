//
//  Buffer.cpp
//  rsed
//
//  Created by David Callahan on 6/27/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#include "LineBuffer.h"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "file_buffer/file_buffer.hpp"
#include <gflags/gflags.h>
#include "rsed.h"
#include "Exception.h"

using std::string;
using std::ifstream;
using std::ofstream;
using std::shared_ptr;
using std::vector;

DEFINE_string(save_prefix, "", "prefix ouf copied input data");
DEFINE_string(replay_prefix, "", "prefix for saved input files");

namespace {

template <typename Stream> class StreamInBuffer : public LineBuffer {
  Stream *stream;

public:
  StreamInBuffer(Stream *stream, std::string name)
      : LineBuffer(name), stream(stream) {
    enableCopy();
  }
  bool eof() override { return stream->eof(); }
  bool getLine(std::string &line) override {
    if (eof()) {
      line = "";
      return false;
    }
    getline(*stream, line);
    if (eof() && line == "") {
      return false;
    }
    inputLine = line;
    lineno += 1;
    return true;
  }
  void append(const std::string &line) override {
    assert(!"invalid append to input buffer");
  }
  void close() override;
};
template class StreamInBuffer<std::istream>;
template <> inline void StreamInBuffer<std::istream>::close() { closed = true; }

template class StreamInBuffer<std::ifstream>;
template <> inline void StreamInBuffer<std::ifstream>::close() {
  stream->close();
  closed = true;
}

template <typename Stream> class StreamOutBuffer : public LineBuffer {
  Stream *stream;

public:
  StreamOutBuffer(Stream *stream, std::string name)
      : LineBuffer(name), stream(stream) {}
  bool eof() override { return false; }
  bool getLine(std::string &line) override {
    assert(!"invalid append to output buffer");
    return false;
  }
  void append(const std::string &line) override { *stream << line << '\n'; }
  void close() override;
};
template class StreamOutBuffer<std::ostream>;
template <> inline void StreamOutBuffer<std::ostream>::close() {
  closed = true;
}
template class StreamOutBuffer<std::ofstream>;
template <> inline void StreamOutBuffer<std::ofstream>::close() {
  stream->close();
  closed = true;
}

class PipeInBuffer : public StreamInBuffer<std::istream> {
public:
  FILE *pipe;
  FILE_buffer fileBuffer;
  std::istream in;
  PipeInBuffer(FILE *pipe, std::string name)
      : StreamInBuffer<std::istream>(&in, name), pipe(pipe),
        fileBuffer(pipe, 8 * 1024), in(&fileBuffer) {
  }
  virtual void close() override {
    if (pipe) {
      int rc = 0;
      try {
        rc = pclose(pipe);
      } catch (...) {
        rc = 1;
      }
      pipe = nullptr;
      if (rc) {
        throw Exception("error in command: " + getName());
      }
    }
    closed = true;
  }
  virtual ~PipeInBuffer() {
    if (!closed) {
      close();
    }
  }
};

class VectorInBuffer : public LineBuffer {
  vector<string> lines;

public:
  VectorInBuffer(vector<string> lines, string name)
      : LineBuffer(name), lines(std::move(lines)) {}
  virtual bool eof() override { return lineno >= lines.size(); }
  virtual bool getLine(std::string &s) override {
    if (lineno < lines.size()) {
      inputLine = std::move(lines[lineno++]);
      s = inputLine;
      return true;
    } else {
      s = "";
      return false;
    }
  }
  virtual void append(const std::string &line) override {
    throw Exception("invalid write to vector input file");
  }
  virtual void close() override {
    lineno = lines.size();
    closed = true;
  }
};

struct Buffer {
  std::shared_ptr<LineBuffer> input = nullptr;
  std::shared_ptr<LineBuffer> output = nullptr;
};
std::unordered_map<std::string, Buffer> buffers;
unsigned inputCount = 0;
string inputFilename(const string &prefix) {
  std::stringstream ss;
  ss << prefix << inputCount << ".in";
  ++inputCount;
  return ss.str();
}

std::shared_ptr<LineBuffer> openInBuffer(std::string fileName) {
  auto f = new std::ifstream(fileName);
  if (!f || !*f) {
    throw Exception("unable to open input file: " + fileName);
  }
  return std::make_shared<StreamInBuffer<std::ifstream>>(f, fileName);
}

std::shared_ptr<LineBuffer> replayFile() {
  auto c = inputCount;
  auto p = openInBuffer(inputFilename(FLAGS_replay_prefix));
  if (RSED::debug) {
    std::cout << "replaying: " << c << ' ' << p->getName()
              << '\n';
  }
  return p;
}

std::shared_ptr<LineBuffer> makeInBuffer(std::istream *stream,
                                         std::string name) {
  return std::make_shared<StreamInBuffer<std::istream>>(stream, name);
}
std::shared_ptr<LineBuffer> makeOutBuffer(std::ostream *stream,
                                          std::string name) {
  return std::make_shared<StreamOutBuffer<std::ostream>>(stream, name);
}
}

std::shared_ptr<LineBuffer> LineBuffer::makeInBuffer(std::string fileName) {
  if (!FLAGS_replay_prefix.empty()) {
    return replayFile();
  }
  return openInBuffer(fileName);
}
std::shared_ptr<LineBuffer> LineBuffer::getStdin() {
  if (!FLAGS_replay_prefix.empty()) {
    assert(inputCount == 0);
    return replayFile();
  }
  return ::makeInBuffer(&std::cin, "<stdin>");
}
std::shared_ptr<LineBuffer> LineBuffer::getStdout() {
  return makeOutBuffer(&std::cout, "<stdout>");
}

std::vector<string> LineBuffer::tempFileNames;
static std::vector<std::shared_ptr<LineBuffer>> pipeFiles;

bool LineBuffer::nextLine(std::string *s) {
  auto rc = getLine(*s);
  if (rc && copyStream.is_open()) {
    copyStream << *s << '\n';
    assert(! copyStream.fail());
  }
  return rc;
}

void LineBuffer::enableCopy() {
  if (!FLAGS_save_prefix.empty()) {
    if (RSED::env_save.is_open()) {
      RSED::env_save << "#input " << inputCount<< " " << name << "\n";
    }
    string saveName = inputFilename(FLAGS_save_prefix);
    copyStream.open(saveName);
    if (!copyStream) {
      throw Exception("unable to open copy output " + saveName);
    }
  }
}

void LineBuffer::closeAll() {
  for (auto &name : LineBuffer::tempFileNames) {
    auto p = buffers.find(name);
    if (p != buffers.end()) {
      Buffer &b = p->second;
      if (b.input && !b.input->closed)
        b.input->close();
      if (b.output && !b.output->closed)
        b.output->close();
      std::remove(name.c_str());
    }
  }
  for (auto &p : pipeFiles) {
    if (!p->closed) {
      p->close();
    }
  }
}

std::shared_ptr<LineBuffer>
LineBuffer::findOutputBuffer(const std::string &name) {
  auto p = buffers.insert(std::make_pair(name, Buffer()));
  Buffer &b = p.first->second;
  if (b.input) {
    if (!b.input->closed) {
      b.input->close();
    }
    b.input = nullptr;
  }
  if (!b.output || b.output->closed) {
    auto f = new ofstream(name);
    if (!*f) {
      string error("unable to open file: ");
      error += name;
      throw error;
    }
    b.output.reset(new StreamOutBuffer<ofstream>(f, name));
  }
  return b.output;
}

std::shared_ptr<LineBuffer>
LineBuffer::findInputBuffer(const std::string &name) {
  auto p = buffers.insert(std::make_pair(name, Buffer()));
  Buffer &b = p.first->second;
  if (b.output) {
    if (!b.output->closed) {
      b.output->close();
    }
    b.output = nullptr;
  }
  if (!b.input || b.input->closed) {
    b.input = makeInBuffer(name);
  }
  return b.input;
}

std::shared_ptr<LineBuffer> LineBuffer::closeBuffer(const std::string &name) {
  auto p = buffers.find(name);
  if (p == buffers.end()) {
    return nullptr;
  }
  Buffer &b = p->second;
  if (b.output) {
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

std::shared_ptr<LineBuffer> LineBuffer::makePipeBuffer(std::string command) {
  if (!FLAGS_replay_prefix.empty()) {
    return replayFile();
  }

  FILE *pipe = nullptr;
  try {
    pipe = popen(command.c_str(), "r");
  } catch (...) {
    throw Exception("error executing command: " + command);
  }
  if (!pipe) {
    throw Exception("error executing command: " + command);
  }
  auto p = std::make_shared<PipeInBuffer>(pipe, command);
  pipeFiles.push_back(p);
  return p;
}

std::shared_ptr<LineBuffer>
LineBuffer::makeVectorInBuffer(std::vector<std::string> *data,
                               std::string name) {
  return std::make_shared<VectorInBuffer>(std::move(*data), name);
}
