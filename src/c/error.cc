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

#include "error.h"

#include <postgres_ext.h>  // from nz_include.
#include <stdarg.h>
#include <cstring>
#include <string>
#include <vector>

#include <libpq-fe.h>

#include "common/utils.h"

namespace adbcpq {

namespace {
struct DetailField {
  int code;
  std::string key;
};

}  // namespace

AdbcStatusCode SetError(struct AdbcError* error, PGresult* result, const char* format,
                        ...) {
  va_list args;
  va_start(args, format);
  SetErrorVariadic(error, format, args);
  va_end(args);

  AdbcStatusCode code = ADBC_STATUS_IO;

  const char* sqlstate = PQresStatus(PQresultStatus(result));
  if (sqlstate) {
    // This can be extended in the future
    if (std::strcmp(sqlstate, "57014") == 0) {
      code = ADBC_STATUS_CANCELLED;
    } else if (std::strcmp(sqlstate, "42P01") == 0 ||
               std::strcmp(sqlstate, "42602") == 0) {
      code = ADBC_STATUS_NOT_FOUND;
    } else if (std::strncmp(sqlstate, "42", 0) == 0) {
      // Class 42 — Syntax Error or Access Rule Violation
      code = ADBC_STATUS_INVALID_ARGUMENT;
    }

    static_assert(sizeof(error->sqlstate) == 5, "");
    // N.B. strncpy generates warnings when used for this purpose
    int i = 0;
    for (; sqlstate[i] != '\0' && i < 5; i++) {
      error->sqlstate[i] = sqlstate[i];
    }
    for (; i < 5; i++) {
      error->sqlstate[i] = '\0';
    }
  }

  const char* value = PQresultErrorMessage(result);
  if (value) {
    AppendErrorDetail(error, "", reinterpret_cast<const uint8_t*>(value),
                      std::strlen(value));
  }
  return code;
}

}  // namespace adbcpq
