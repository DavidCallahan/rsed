// Copyright 2007 Edd Dawson.
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file BOOST_LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#include "file_buffer.hpp"

#include <algorithm>
#include <cstring>

using std::size_t;

FILE_buffer::FILE_buffer(FILE *fptr, size_t buff_sz, size_t put_back) :
	fptr_(fptr),
	put_back_(std::max(put_back, size_t(1))),
	buffer_(std::max(buff_sz, put_back_) + put_back_)
{
	// Set the back, current and end buffer pointers to be equal.
	// This will force an underflow() on the first read and hence
	// fill the buffer.
	char *end = &buffer_.front() + buffer_.size();
	setg(end, end, end);
}

std::streambuf::int_type FILE_buffer::underflow()
{
	if (gptr() < egptr()) // buffer not exhausted
		return traits_type::to_int_type(*gptr());

	char *base = &buffer_.front();
	char *start = base;

	if (eback() == base)
	{
		// Make arrangements for putback characters
		std::memmove(base, egptr() - put_back_, put_back_);
		start += put_back_;
	}

	// start is now the start of the buffer, proper.
	// Read from fptr_ in to the provided buffer
	size_t n = std::fread(start, 1, buffer_.size() - (start - base), fptr_);
	if (n == 0)
		return traits_type::eof();

	// Set buffer pointers
	setg(base, start, start + n);
	
	return traits_type::to_int_type(*gptr());
}
