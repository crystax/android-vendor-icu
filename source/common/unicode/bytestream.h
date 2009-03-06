// Copyright (C) 2009, International Business Machines
// Corporation and others. All Rights Reserved.
//
// Copyright 2007 Google Inc. All Rights Reserved.
// Author: sanjay@google.com (Sanjay Ghemawat)
//
// Abstract interface that consumes a sequence of bytes (ByteSink).
//
// Used so that we can write a single piece of code that can operate
// on a variety of output string types.
//
// Various implementations of this interface are provided:
//   ByteSink:
//      CheckedArrayByteSink    Write to a flat array, with bounds checking
//      StringByteSink          Write to an STL string

#ifndef __BYTESTREAM_H__
#define __BYTESTREAM_H__

/**
 * \file 
 * \brief C++ API: Interface for writing bytes, and implementation classes.
 */

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "unicode/std_string.h"

U_NAMESPACE_BEGIN

// A ByteSink can be filled with bytes
// @draft ICU 4.2
class U_COMMON_API ByteSink : public UMemory {
public:
  // @draft ICU 4.2
  ByteSink() { }
  // @draft ICU 4.2
  virtual ~ByteSink() { }

  // Append "bytes[0,n-1]" to this.
  // @draft ICU 4.2
  virtual void Append(const char* bytes, int32_t n) = 0;

  // Returns a writable buffer for appending and writes the buffer's capacity to
  // *result_capacity. Guarantees *result_capacity>=min_capacity.
  // May return a pointer to the caller-owned scratch buffer which must have
  // scratch_capacity>=min_capacity.
  // The returned buffer is only valid until the next operation
  // on this ByteSink.
  //
  // After writing at most *result_capacity bytes, call Append() with the
  // pointer returned from this function and the number of bytes written.
  // Many Append() implementations will avoid copying bytes if this function
  // returned an internal buffer.
  //
  // Partial usage example:
  //  int32_t capacity;
  //  char* buffer = sink->GetAppendBuffer(..., &capacity);
  //  ... Write n bytes into buffer, with n <= capacity.
  //  sink->Append(buffer, n);
  // In many implementations, that call to Append will avoid copying bytes.
  //
  // If the ByteSink allocates or reallocates an internal buffer, it should use
  // the desired_capacity_hint if appropriate.
  // If a caller cannot provide a reasonable guess at the desired capacity,
  // it should pass desired_capacity_hint=0.
  //
  // If a non-scratch buffer is returned, the caller may only pass
  // a prefix to it to Append().
  // That is, it is not correct to pass an interior pointer to Append().
  //
  // The default implementation always returns the scratch buffer.
  // @draft ICU 4.2
  virtual char* GetAppendBuffer(int32_t min_capacity,
                                int32_t desired_capacity_hint,
                                char* scratch, int32_t scratch_capacity,
                                int32_t* result_capacity);

  // Flush internal buffers.
  // Some byte sinks use internal buffers or provide buffering
  // and require calling Flush() at the end of the stream.
  // The default implementation of Flush() does nothing.
  // @draft ICU 4.2
  virtual void Flush();

private:
  ByteSink(const ByteSink &); // copy constructor not implemented
  ByteSink &operator=(const ByteSink &); // assignment operator not implemented
};

// -------------------------------------------------------------
// Some standard implementations

// Implementation of ByteSink that writes to a flat byte array,
// with bounds-checking:
// This sink will not write more than capacity bytes to outbuf.
// If more than capacity bytes are Append()ed, then excess bytes are ignored,
// and Overflowed() will return true.
// Overflow does not cause a runtime error.
// @draft ICU 4.2
class U_COMMON_API CheckedArrayByteSink : public ByteSink {
public:
  // @draft ICU 4.2
  CheckedArrayByteSink(char* outbuf, int32_t capacity);
  // @draft ICU 4.2
  virtual void Append(const char* bytes, int32_t n);
  // @draft ICU 4.2
  virtual char* GetAppendBuffer(int32_t min_capacity,
                                int32_t desired_capacity_hint,
                                char* scratch, int32_t scratch_capacity,
                                int32_t* result_capacity);
  // Returns the number of bytes actually written to the sink.
  // @draft ICU 4.2
  int32_t NumberOfBytesWritten() const { return size_; }
  // Returns true if any bytes were discarded, i.e., if there was an
  // attempt to write more than 'capacity' bytes.
  // @draft ICU 4.2
  UBool Overflowed() const { return overflowed_; }
private:
  char* outbuf_;
  const int32_t capacity_;
  int32_t size_;
  bool overflowed_;
  CheckedArrayByteSink(); // default constructor not implemented
  CheckedArrayByteSink(const CheckedArrayByteSink &); // copy constructor not implemented
  CheckedArrayByteSink &operator=(const CheckedArrayByteSink &); // assignment operator not implemented
};

#if U_HAVE_STD_STRING

// Implementation of ByteSink that writes to a "string".
// @draft ICU 4.2
template<typename StringClass>
class StringByteSink : public ByteSink {
 public:
  // @draft ICU 4.2
  StringByteSink(StringClass* dest) : dest_(dest) { }
  // @draft ICU 4.2
  virtual void Append(const char* data, int32_t n) { dest_->append(data, n); }
 private:
  StringClass* dest_;
  StringByteSink(); // default constructor not implemented
  StringByteSink(const StringByteSink &); // copy constructor not implemented
  StringByteSink &operator=(const StringByteSink &); // assignment operator not implemented
};

#endif

U_NAMESPACE_END

#endif  // __BYTESTREAM_H__
