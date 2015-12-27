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
class LineBuffer {
protected:
  int lineno;
  std::string name;
  std::string inputLine;

public:
  LineBuffer(std::string name) : lineno(0), name(name) {}
  int getLineno() const { return lineno; }
  virtual bool eof() = 0;
  virtual bool getLine(std::string &) = 0;
  virtual void append(const std::string &line) = 0;
  virtual void close() = 0;
  const std::string &getName() const { return name; }
  virtual ~LineBuffer() {}

  const std::string &getInputLine() { return inputLine; }

  static std::shared_ptr<LineBuffer> findOutputBuffer(const std::string &);
  static std::shared_ptr<LineBuffer> findInputBuffer(const std::string &);
  static void removeTempFiles(const std::vector<std::string> &names);
  static std::shared_ptr<LineBuffer> closeBuffer(const std::string &);
  static std::vector<std::string> tempFileNames;
  
  template<typename Stream>
  static std::shared_ptr<LineBuffer> makeInBuffer(Stream *, std::string);
  template<typename Stream>
  static std::shared_ptr<LineBuffer> makeOutBuffer(Stream *, std::string);
  
  static std::shared_ptr<LineBuffer> makePipeBuffer(std::string command);
};

class LineBufferCloser {
public:
  ~LineBufferCloser();
};


#endif /* defined(__rsed__LineBuffer__) */
