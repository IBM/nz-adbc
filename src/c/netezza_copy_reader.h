// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

// Windows
#define NOMINMAX

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nanoarrow/nanoarrow.hpp>

#include "common/utils.h"
#include "netezza_type.h"
#include "netezza_util.h"

// R 3.6 / Windows builds on a very old toolchain that does not define ENODATA
#if defined(_WIN32) && !defined(MSVC) && !defined(ENODATA)
#define ENODATA 120
#endif

namespace adbcpq {

// "PGCOPY\n\377\r\n\0"
// static int8_t kPgCopyBinarySignature[] = {0x50, 0x47, 0x43, 0x4F,
//        			  0x50, 0x59, 0x0A, static_cast<int8_t>(0xFF),
//        			  0x0D, 0x0A, 0x00};

// The maximum value in seconds that can be converted into microseconds
// without overflow
constexpr int64_t kMaxSafeSecondsToMicros = 9223372036854L;

// The minimum value in seconds that can be converted into microseconds
// without overflow
constexpr int64_t kMinSafeSecondsToMicros = -9223372036854L;

// The maximum value in milliseconds that can be converted into microseconds
// without overflow
constexpr int64_t kMaxSafeMillisToMicros = 9223372036854775L;

// The minimum value in milliseconds that can be converted into microseconds
// without overflow
constexpr int64_t kMinSafeMillisToMicros = -9223372036854775L;

// The maximum value in microseconds that can be converted into nanoseconds
// without overflow
constexpr int64_t kMaxSafeMicrosToNanos = 9223372036854775L;

// The minimum value in microseconds that can be converted into nanoseconds
// without overflow
constexpr int64_t kMinSafeMicrosToNanos = -9223372036854775L;

// 2000-01-01 00:00:00.000000 in microseconds
constexpr int64_t kNetezzaTimestampEpoch = 946684800000000L;

// Read a value from the buffer without checking the buffer size. Advances
// the cursor of data and reduces its size by sizeof(T).
template <typename T>
inline T ReadUnsafe(ArrowBufferView* data) {
        T out;
        memcpy(&out, data->data.data, sizeof(T));
        out = SwapNetworkToHost(out);
        data->data.as_uint8 += sizeof(T);
        data->size_bytes -= sizeof(T);
        return out;
}

// Define some explicit specializations for types that don't have a SwapNetworkToHost
// overload.
template <>
inline int8_t ReadUnsafe(ArrowBufferView* data) {
        int8_t out = data->data.as_int8[0];
        data->data.as_uint8 += sizeof(int8_t);
        data->size_bytes -= sizeof(int8_t);
        return out;
}

template <>
inline int16_t ReadUnsafe(ArrowBufferView* data) {
        return static_cast<int16_t>(ReadUnsafe<uint16_t>(data));
}

template <>
inline int32_t ReadUnsafe(ArrowBufferView* data) {
        return static_cast<int32_t>(ReadUnsafe<uint32_t>(data));
}

template <>
inline int64_t ReadUnsafe(ArrowBufferView* data) {
        return static_cast<int64_t>(ReadUnsafe<uint64_t>(data));
}

template <typename T>
ArrowErrorCode ReadChecked(ArrowBufferView* data, T* out, ArrowError* error) {
        if (data->size_bytes < static_cast<int64_t>(sizeof(T))) {
          ArrowErrorSet(error, "Unexpected end of input (expected %d bytes but found %ld)",
          static_cast<int>(sizeof(T)),
          static_cast<long>(data->size_bytes));  // NOLINT(runtime/int)
          return EINVAL;
        }

        *out = ReadUnsafe<T>(data);
        return NANOARROW_OK;
}

// Write a value to a buffer without checking the buffer size. Advances
// the cursor of buffer and reduces it by sizeof(T)
template <typename T>
inline void WriteUnsafe(ArrowBuffer* buffer, T in) {
        const T value = SwapNetworkToHost(in);
        ArrowBufferAppendUnsafe(buffer, &value, sizeof(T));
}

template <>
inline void WriteUnsafe(ArrowBuffer* buffer, int8_t in) {
        ArrowBufferAppendUnsafe(buffer, &in, sizeof(int8_t));
}

template <>
inline void WriteUnsafe(ArrowBuffer* buffer, int16_t in) {
        WriteUnsafe<uint16_t>(buffer, in);
}

template <>
inline void WriteUnsafe(ArrowBuffer* buffer, int32_t in) {
        WriteUnsafe<uint32_t>(buffer, in);
}

template <>
inline void WriteUnsafe(ArrowBuffer* buffer, int64_t in) {
        WriteUnsafe<uint64_t>(buffer, in);
}

template <typename T>
ArrowErrorCode WriteChecked(ArrowBuffer* buffer, T in, ArrowError* error) {
        NANOARROW_RETURN_NOT_OK(ArrowBufferReserve(buffer, sizeof(T)));
        WriteUnsafe<T>(buffer, in);
        return NANOARROW_OK;
}

class NetezzaCopyFieldReader {
 public:
        NetezzaCopyFieldReader() : validity_(nullptr), offsets_(nullptr), data_(nullptr) {};

        virtual ~NetezzaCopyFieldReader() {};
        void Init(const NetezzaType& pg_type);
        const NetezzaType& InputType() const;
        virtual ArrowErrorCode InitSchema(ArrowSchema* schema);
        virtual ArrowErrorCode InitArray(ArrowArray* array);
        virtual ArrowErrorCode Read(ArrowBufferView* data, int32_t field_size_bytes,
        	      ArrowArray* array, ArrowError* error);
        virtual ArrowErrorCode FinishArray(ArrowArray* array, ArrowError* error);

 protected:
        NetezzaType pg_type_;
        ArrowSchemaView schema_view_;
        ArrowBitmap* validity_;
        ArrowBuffer* offsets_;
        ArrowBuffer* data_;
        std::vector<std::unique_ptr<NetezzaCopyFieldReader>> children_;

        ArrowErrorCode AppendValid(ArrowArray* array);
};

// Reader for a Netezza boolean (one byte -> bitmap)
class NetezzaCopyBooleanFieldReader : public NetezzaCopyFieldReader {
 public:
        ArrowErrorCode Read(ArrowBufferView* data, int32_t field_size_bytes, ArrowArray* array,
              ArrowError* error) override;
};

// Reader for Pg->Arrow conversions whose representations are identical minus
// the bswap from network endian. This includes all integral and float types.
template <typename T, T kOffset = 0>
class NetezzaCopyNetworkEndianFieldReader : public NetezzaCopyFieldReader {
 public:
        ArrowErrorCode Read(ArrowBufferView* data, int32_t field_size_bytes, ArrowArray* array,
              ArrowError* error) override;
};

// Reader for Intervals
class NetezzaCopyIntervalFieldReader : public NetezzaCopyFieldReader {
 public:
        ArrowErrorCode Read(ArrowBufferView* data, int32_t field_size_bytes, ArrowArray* array,
              ArrowError* error) override;
};

// // Converts COPY resulting from the Netezza NUMERIC type into a string.
// Rewritten based on the Netezza implementation of NUMERIC cast to string in
// src/backend/utils/adt/numeric.c : get_str_from_var() (Note that in the initial source,
// DEC_DIGITS is always 4 and DBASE is always 10000).
//
// Briefly, the Netezza representation of "numeric" is an array of int16_t ("digits")
// from most significant to least significant. Each "digit" is a value between 0000 and
// 9999. There are weight + 1 digits before the decimal point and dscale digits after the
// decimal point. Both of those values can be zero or negative. A "sign" component
// encodes the positive or negativeness of the value and is also used to encode special
// values (inf, -inf, and nan).
class NetezzaCopyNumericFieldReader : public NetezzaCopyFieldReader {
 public:
        ArrowErrorCode Read(ArrowBufferView* data, int32_t field_size_bytes, ArrowArray* array,
              ArrowError* error) override;
 private:
        std::vector<int16_t> digits_;

        // Number of decimal digits per Netezza digit
        static const int kDecDigits = 4;
        // The "base" of the Netezza representation (i.e., each "digit" is 0 to 9999)
        static const int kNBase = 10000;
        // Valid values for the sign component
        static const uint16_t kNumericPos = 0x0000;
        static const uint16_t kNumericNeg = 0x4000;
        static const uint16_t kNumericNAN = 0xC000;
        static const uint16_t kNumericPinf = 0xD000;
        static const uint16_t kNumericNinf = 0xF000;
};

// Reader for Pg->Arrow conversions whose Arrow representation is simply the
// bytes of the field representation. This can be used with binary and string
// Arrow types and any Netezza type.
class NetezzaCopyBinaryFieldReader : public NetezzaCopyFieldReader {
 public:
        ArrowErrorCode Read(ArrowBufferView* data, int32_t field_size_bytes, ArrowArray* array,
              ArrowError* error) override; 
};

class NetezzaCopyArrayFieldReader : public NetezzaCopyFieldReader {
 public:
        void InitChild(std::unique_ptr<NetezzaCopyFieldReader> child);
        ArrowErrorCode InitSchema(ArrowSchema* schema) override;
        ArrowErrorCode InitArray(ArrowArray* array) override;
        ArrowErrorCode Read(ArrowBufferView* data, int32_t field_size_bytes, ArrowArray* array,
              ArrowError* error) override;
 private:
        std::unique_ptr<NetezzaCopyFieldReader> child_;
};

class NetezzaCopyRecordFieldReader : public NetezzaCopyFieldReader {
 public:
        void AppendChild(std::unique_ptr<NetezzaCopyFieldReader> child);
        ArrowErrorCode InitSchema(ArrowSchema* schema) override;
        ArrowErrorCode InitArray(ArrowArray* array) override;
        ArrowErrorCode Read(ArrowBufferView* data, int32_t field_size_bytes, ArrowArray* array,
              ArrowError* error) override;
 private:
        std::vector<std::unique_ptr<NetezzaCopyFieldReader>> children_;
};

// Subtely different from a Record field item: field count is an int16_t
// instead of an int32_t and each field is not prefixed by its OID.
class NetezzaCopyFieldTupleReader : public NetezzaCopyFieldReader {
 public:
        void AppendChild(std::unique_ptr<NetezzaCopyFieldReader> child);
        ArrowErrorCode InitSchema(ArrowSchema* schema) override;
        ArrowErrorCode InitArray(ArrowArray* array) override;
        ArrowErrorCode Read(ArrowBufferView* data, int32_t field_size_bytes, ArrowArray* array,
              ArrowError* error) override;
 private:
        std::vector<std::unique_ptr<NetezzaCopyFieldReader>> children_;
};

// Factory for a NetezzaCopyFieldReader that instantiates the proper subclass
// and gives a nice error for Netezza type -> Arrow type conversions that aren't
// supported.
 ArrowErrorCode ErrorCantConvert(ArrowError* error,
        			      const NetezzaType& pg_type,
        			      const ArrowSchemaView& schema_view);

 ArrowErrorCode MakeCopyFieldReader(const NetezzaType& pg_type,
        				 ArrowSchema* schema,
        				 NetezzaCopyFieldReader** out,
        				 ArrowError* error);

class NetezzaCopyStreamReader {
 public:
        ArrowErrorCode Init(NetezzaType pg_type);
        int64_t array_size_approx_bytes() const;
        ArrowErrorCode SetOutputSchema(ArrowSchema* schema, ArrowError* error);
        ArrowErrorCode InferOutputSchema(ArrowError* error);
        ArrowErrorCode InitFieldReaders(ArrowError* error);
        ArrowErrorCode ReadHeader(ArrowBufferView* data, ArrowError* error);
        ArrowErrorCode ReadRecord(ArrowBufferView* data, ArrowError* error);
        ArrowErrorCode GetSchema(ArrowSchema* out);
        ArrowErrorCode GetArray(ArrowArray* out, ArrowError* error);
        const NetezzaType& pg_type() const;

 private:
        NetezzaType pg_type_;
        NetezzaCopyFieldTupleReader root_reader_;
        nanoarrow::UniqueSchema schema_;
        nanoarrow::UniqueArray array_;
        int64_t array_size_approx_bytes_;
};

class NetezzaCopyFieldWriter {
 public:
        virtual ~NetezzaCopyFieldWriter() {};

        void Init(struct ArrowArrayView* array_view);

        virtual ArrowErrorCode Write(ArrowBuffer* buffer, int64_t index, ArrowError* error);

 protected:
        struct ArrowArrayView* array_view_;
        std::vector<std::unique_ptr<NetezzaCopyFieldWriter>> children_;
};

class NetezzaCopyFieldTupleWriter : public NetezzaCopyFieldWriter {
 public:
        void AppendChild(std::unique_ptr<NetezzaCopyFieldWriter> child);
        ArrowErrorCode Write(ArrowBuffer* buffer, int64_t index, ArrowError* error) override;
 private:
        std::vector<std::unique_ptr<NetezzaCopyFieldWriter>> children_;
};

class NetezzaCopyBooleanFieldWriter : public NetezzaCopyFieldWriter {
 public:
        ArrowErrorCode Write(ArrowBuffer* buffer, int64_t index, ArrowError* error) override;
};

template <typename T, T kOffset = 0>
class NetezzaCopyNetworkEndianFieldWriter : public NetezzaCopyFieldWriter {
 public:
        ArrowErrorCode Write(ArrowBuffer* buffer, int64_t index, ArrowError* error) override;
};

class NetezzaCopyFloatFieldWriter : public NetezzaCopyFieldWriter {
 public:
        ArrowErrorCode Write(ArrowBuffer* buffer, int64_t index, ArrowError* error) override;
};

class NetezzaCopyDoubleFieldWriter : public NetezzaCopyFieldWriter {
 public:
        ArrowErrorCode Write(ArrowBuffer* buffer, int64_t index, ArrowError* error) override;
};

class NetezzaCopyIntervalFieldWriter : public NetezzaCopyFieldWriter {
 public:
        ArrowErrorCode Write(ArrowBuffer* buffer, int64_t index, ArrowError* error) override;
};

template <enum ArrowTimeUnit TU>
class NetezzaCopyDurationFieldWriter : public NetezzaCopyFieldWriter {
 public:
        ArrowErrorCode Write(ArrowBuffer* buffer, int64_t index, ArrowError* error) override;
};

class NetezzaCopyBinaryFieldWriter : public NetezzaCopyFieldWriter {
 public:
        ArrowErrorCode Write(ArrowBuffer* buffer, int64_t index, ArrowError* error) override;
};

class NetezzaCopyBinaryDictFieldWriter : public NetezzaCopyFieldWriter {
 public:
        ArrowErrorCode Write(ArrowBuffer* buffer, int64_t index, ArrowError* error) override;
};

template <enum ArrowTimeUnit TU>
class NetezzaCopyTimestampFieldWriter : public NetezzaCopyFieldWriter {
 public:
        ArrowErrorCode Write(ArrowBuffer* buffer, int64_t index, ArrowError* error) override;
};

 ArrowErrorCode MakeCopyFieldWriter(struct ArrowSchema* schema,
        				 NetezzaCopyFieldWriter** out,
        				 ArrowError* error);

class NetezzaCopyStreamWriter {
 public:
            ArrowErrorCode Init(struct ArrowSchema* schema);
        ArrowErrorCode SetArray(struct ArrowArray* array);
        ArrowErrorCode WriteHeader(ArrowError* error);
        ArrowErrorCode WriteRecord(ArrowError* error);

        ArrowErrorCode InitFieldWriters(ArrowError* error);
        

        const struct ArrowBuffer& WriteBuffer() const;

        void Rewind();

 private:
        NetezzaCopyFieldTupleWriter root_writer_;
        struct ArrowSchema* schema_;
        Handle<struct ArrowArrayView> array_view_;
        Handle<struct ArrowBuffer> buffer_;
        int64_t records_written_ = 0;
};

} // namespace adbcpq
