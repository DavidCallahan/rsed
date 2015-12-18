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
class LineBuffer {
protected:
  int lineno;
  std::string name;
public:
  LineBuffer(std::string name) : lineno(0), name(name) {}
  int getLineno() const { return lineno; }
  virtual bool eof() = 0;
  virtual bool getLine(std::string &) = 0;
  virtual void append(const std::string &line) = 0;
  virtual void close() = 0;
  const std::string &getName() const { return name; }
  virtual bool rewind() { return false; }
  virtual ~LineBuffer() {}

  static LineBuffer *findOutputBuffer(const std::string &);
  static LineBuffer *findInputBuffer(const std::string &);
  static void removeTempFiles(const std::vector<std::string> &names);
  static std::vector<std::string> tempFileNames;
};

class LineBufferCloser {
public:
  ~LineBufferCloser();
};

template <typename Stream> class StreamInBuffer : public LineBuffer {
  Stream *stream;

public:
  StreamInBuffer(Stream *stream, std::string name)
      : LineBuffer(name), stream(stream) {}
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
    lineno += 1;
    return true;
  }
  void append(const std::string &line) override {
    assert(!"invalid append to input buffer");
  }
  void close() override;
  bool rewind() override;
};
template class StreamInBuffer<std::istream>;
template <> inline void StreamInBuffer<std::istream>::close() {}
template <> inline bool StreamInBuffer<std::istream>::rewind() { return false; }

template class StreamInBuffer<std::ifstream>;
template <> inline void StreamInBuffer<std::ifstream>::close() {
  stream->close();
}

template <typename Stream>
inline StreamInBuffer<Stream> *makeInBuffer(Stream *stream, std::string name) {
  return new StreamInBuffer<Stream>(stream, name);
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
template <> inline void StreamOutBuffer<std::ostream>::close() {}
template class StreamOutBuffer<std::ofstream>;
template <> inline void StreamOutBuffer<std::ofstream>::close() {
  stream->close();
}
template <typename Stream>
    inline StreamOutBuffer<Stream> *makeOutBuffer(Stream *stream, std::string name) {
  return new StreamOutBuffer<Stream>(stream,name);
}

#endif /* defined(__rsed__LineBuffer__) */
