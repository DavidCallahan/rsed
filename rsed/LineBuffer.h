//
//  LineBuffer.h
//  rsed
//
//  Created by David Callahan on 6/27/15.
//  Copyright (c) 2015 David Callahan. All rights reserved.
//

#ifndef __rsed__LineBuffer__
#define __rsed__LineBuffer__
#include <assert.h>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "StringRef.h"

class LineBuffer {
  std::ofstream copyStream;
  std::string name;
  virtual bool getLine() = 0;

protected:
  bool closed = false;
  StringPtr inputLine;
  int lineno = 0;
  void enableCopy();

public:
  LineBuffer(std::string name) : name(name) {}
  int getLineno() const { return lineno; }
  const std::string &getName() const { return name; }

  bool nextLine();
  virtual bool eof() = 0;
  virtual void appendLine(const std::string &line) = 0;
  virtual void appendString(const std::string &word) = 0;
  virtual void close() = 0;
  virtual ~LineBuffer();

  StringPtr getInputLine() { return inputLine; }

  static std::shared_ptr<LineBuffer> findOutputBuffer(const std::string &);
  static std::shared_ptr<LineBuffer> findInputBuffer(const std::string &);
  static void removeTempFiles(const std::vector<std::string> &names);
  static std::shared_ptr<LineBuffer> closeBuffer(const std::string &);
  static std::vector<std::string> tempFileNames;

  static std::shared_ptr<LineBuffer> makeInBuffer(std::string);
  static std::shared_ptr<LineBuffer> makePipeBuffer(std::string command);
  static std::shared_ptr<LineBuffer>
  makeVectorInBuffer(std::vector<std::string> *data, std::string name);
  static std::shared_ptr<LineBuffer> getStdin();
  static std::shared_ptr<LineBuffer> getStdout();
  static void closeAll();
};

#endif /* defined(__rsed__LineBuffer__) */
