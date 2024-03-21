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

// A libpq-based PostgreSQL driver for ADBC.

#include <cstring>
#include <memory>

#include <adbc.h>

#include "common/utils.h"
#include "connection.h"
#include "database.h"
#include "statement.h"

using adbcpq::NetezzaConnection;
using adbcpq::NetezzaDatabase;
using adbcpq::NetezzaStatement;

// ---------------------------------------------------------------------
// ADBC interface implementation - as private functions so that these
// don't get replaced by the dynamic linker. If we implemented these
// under the Adbc* names, then in DriverInit, the linker may resolve
// functions to the address of the functions provided by the driver
// manager instead of our functions.
//
// We could also:
// - Play games with RTLD_DEEPBIND - but this doesn't work with ASan
// - Use __attribute__((visibility("protected"))) - but this is
//   apparently poorly supported by some linkers
// - Play with -Bsymbolic(-functions) - but this has other
//   consequences and complicates the build setup
//
// So in the end some manual effort here was chosen.

// ---------------------------------------------------------------------
// AdbcError

namespace {
const struct AdbcError* NetezzaErrorFromArrayStream(struct ArrowArrayStream* stream,
                                                     AdbcStatusCode* status) {
  // Currently only valid for TupleReader
  return adbcpq::TupleReader::ErrorFromArrayStream(stream, status);
}
}  // namespace

int AdbcErrorGetDetailCount(const struct AdbcError* error) {
  return CommonErrorGetDetailCount(error);
}

struct AdbcErrorDetail AdbcErrorGetDetail(const struct AdbcError* error, int index) {
  return CommonErrorGetDetail(error, index);
}

const struct AdbcError* AdbcErrorFromArrayStream(struct ArrowArrayStream* stream,
                                                 AdbcStatusCode* status) {
  return NetezzaErrorFromArrayStream(stream, status);
}

// ---------------------------------------------------------------------
// AdbcDatabase

namespace {
AdbcStatusCode NetezzaDatabaseInit(struct AdbcDatabase* database,
                                    struct AdbcError* error) {
  if (!database || !database->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr = reinterpret_cast<std::shared_ptr<NetezzaDatabase>*>(database->private_data);
  return (*ptr)->Init(error);
}

AdbcStatusCode NetezzaDatabaseNew(struct AdbcDatabase* database,
                                   struct AdbcError* error) {
  if (!database) {
    SetError(error, "%s", "[libpq] database must not be null");
    return ADBC_STATUS_INVALID_STATE;
  }
  if (database->private_data) {
    SetError(error, "%s", "[libpq] database is already initialized");
    return ADBC_STATUS_INVALID_STATE;
  }
  auto impl = std::make_shared<NetezzaDatabase>();
  database->private_data = new std::shared_ptr<NetezzaDatabase>(impl);
  return ADBC_STATUS_OK;
}

AdbcStatusCode NetezzaDatabaseRelease(struct AdbcDatabase* database,
                                       struct AdbcError* error) {
  if (!database->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr = reinterpret_cast<std::shared_ptr<NetezzaDatabase>*>(database->private_data);
  AdbcStatusCode status = (*ptr)->Release(error);
  delete ptr;
  database->private_data = nullptr;
  return status;
}

AdbcStatusCode NetezzaDatabaseGetOption(struct AdbcDatabase* database, const char* key,
                                         char* value, size_t* length,
                                         struct AdbcError* error) {
  if (!database->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr = reinterpret_cast<std::shared_ptr<NetezzaDatabase>*>(database->private_data);
  return (*ptr)->GetOption(key, value, length, error);
}

AdbcStatusCode NetezzaDatabaseGetOptionBytes(struct AdbcDatabase* database,
                                              const char* key, uint8_t* value,
                                              size_t* length, struct AdbcError* error) {
  if (!database->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr = reinterpret_cast<std::shared_ptr<NetezzaDatabase>*>(database->private_data);
  return (*ptr)->GetOptionBytes(key, value, length, error);
}

AdbcStatusCode NetezzaDatabaseGetOptionDouble(struct AdbcDatabase* database,
                                               const char* key, double* value,
                                               struct AdbcError* error) {
  if (!database->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr = reinterpret_cast<std::shared_ptr<NetezzaDatabase>*>(database->private_data);
  return (*ptr)->GetOptionDouble(key, value, error);
}

AdbcStatusCode NetezzaDatabaseGetOptionInt(struct AdbcDatabase* database,
                                            const char* key, int64_t* value,
                                            struct AdbcError* error) {
  if (!database->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr = reinterpret_cast<std::shared_ptr<NetezzaDatabase>*>(database->private_data);
  return (*ptr)->GetOptionInt(key, value, error);
}

AdbcStatusCode NetezzaDatabaseSetOption(struct AdbcDatabase* database, const char* key,
                                         const char* value, struct AdbcError* error) {
  if (!database || !database->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr = reinterpret_cast<std::shared_ptr<NetezzaDatabase>*>(database->private_data);
  return (*ptr)->SetOption(key, value, error);
}

AdbcStatusCode NetezzaDatabaseSetOptionBytes(struct AdbcDatabase* database,
                                              const char* key, const uint8_t* value,
                                              size_t length, struct AdbcError* error) {
  if (!database->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr = reinterpret_cast<std::shared_ptr<NetezzaDatabase>*>(database->private_data);
  return (*ptr)->SetOptionBytes(key, value, length, error);
}

AdbcStatusCode NetezzaDatabaseSetOptionDouble(struct AdbcDatabase* database,
                                               const char* key, double value,
                                               struct AdbcError* error) {
  if (!database->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr = reinterpret_cast<std::shared_ptr<NetezzaDatabase>*>(database->private_data);
  return (*ptr)->SetOptionDouble(key, value, error);
}

AdbcStatusCode NetezzaDatabaseSetOptionInt(struct AdbcDatabase* database,
                                            const char* key, int64_t value,
                                            struct AdbcError* error) {
  if (!database->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr = reinterpret_cast<std::shared_ptr<NetezzaDatabase>*>(database->private_data);
  return (*ptr)->SetOptionInt(key, value, error);
}
}  // namespace

AdbcStatusCode AdbcDatabaseGetOption(struct AdbcDatabase* database, const char* key,
                                     char* value, size_t* length,
                                     struct AdbcError* error) {
  return NetezzaDatabaseGetOption(database, key, value, length, error);
}

AdbcStatusCode AdbcDatabaseGetOptionBytes(struct AdbcDatabase* database, const char* key,
                                          uint8_t* value, size_t* length,
                                          struct AdbcError* error) {
  return NetezzaDatabaseGetOptionBytes(database, key, value, length, error);
}

AdbcStatusCode AdbcDatabaseGetOptionInt(struct AdbcDatabase* database, const char* key,
                                        int64_t* value, struct AdbcError* error) {
  return NetezzaDatabaseGetOptionInt(database, key, value, error);
}

AdbcStatusCode AdbcDatabaseGetOptionDouble(struct AdbcDatabase* database, const char* key,
                                           double* value, struct AdbcError* error) {
  return NetezzaDatabaseGetOptionDouble(database, key, value, error);
}

AdbcStatusCode AdbcDatabaseInit(struct AdbcDatabase* database, struct AdbcError* error) {
  return NetezzaDatabaseInit(database, error);
}

AdbcStatusCode AdbcDatabaseNew(struct AdbcDatabase* database, struct AdbcError* error) {
  return NetezzaDatabaseNew(database, error);
}

AdbcStatusCode AdbcDatabaseRelease(struct AdbcDatabase* database,
                                   struct AdbcError* error) {
  return NetezzaDatabaseRelease(database, error);
}

AdbcStatusCode AdbcDatabaseSetOption(struct AdbcDatabase* database, const char* key,
                                     const char* value, struct AdbcError* error) {
  return NetezzaDatabaseSetOption(database, key, value, error);
}

AdbcStatusCode AdbcDatabaseSetOptionBytes(struct AdbcDatabase* database, const char* key,
                                          const uint8_t* value, size_t length,
                                          struct AdbcError* error) {
  return NetezzaDatabaseSetOptionBytes(database, key, value, length, error);
}

AdbcStatusCode AdbcDatabaseSetOptionInt(struct AdbcDatabase* database, const char* key,
                                        int64_t value, struct AdbcError* error) {
  return NetezzaDatabaseSetOptionInt(database, key, value, error);
}

AdbcStatusCode AdbcDatabaseSetOptionDouble(struct AdbcDatabase* database, const char* key,
                                           double value, struct AdbcError* error) {
  return NetezzaDatabaseSetOptionDouble(database, key, value, error);
}

// ---------------------------------------------------------------------
// AdbcConnection

namespace {
AdbcStatusCode NetezzaConnectionCancel(struct AdbcConnection* connection,
                                        struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->Cancel(error);
}

AdbcStatusCode NetezzaConnectionCommit(struct AdbcConnection* connection,
                                        struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->Commit(error);
}

AdbcStatusCode NetezzaConnectionGetInfo(struct AdbcConnection* connection,
                                         const uint32_t* info_codes,
                                         size_t info_codes_length,
                                         struct ArrowArrayStream* stream,
                                         struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->GetInfo(connection, info_codes, info_codes_length, stream, error);
}

AdbcStatusCode NetezzaConnectionGetObjects(
    struct AdbcConnection* connection, int depth, const char* catalog,
    const char* db_schema, const char* table_name, const char** table_types,
    const char* column_name, struct ArrowArrayStream* stream, struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->GetObjects(connection, depth, catalog, db_schema, table_name,
                            table_types, column_name, stream, error);
}

AdbcStatusCode NetezzaConnectionGetOption(struct AdbcConnection* connection,
                                           const char* key, char* value, size_t* length,
                                           struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->GetOption(key, value, length, error);
}

AdbcStatusCode NetezzaConnectionGetOptionBytes(struct AdbcConnection* connection,
                                                const char* key, uint8_t* value,
                                                size_t* length, struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->GetOptionBytes(key, value, length, error);
}

AdbcStatusCode NetezzaConnectionGetOptionDouble(struct AdbcConnection* connection,
                                                 const char* key, double* value,
                                                 struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->GetOptionDouble(key, value, error);
}

AdbcStatusCode NetezzaConnectionGetOptionInt(struct AdbcConnection* connection,
                                              const char* key, int64_t* value,
                                              struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->GetOptionInt(key, value, error);
}

AdbcStatusCode NetezzaConnectionGetStatistics(struct AdbcConnection* connection,
                                               const char* catalog, const char* db_schema,
                                               const char* table_name, char approximate,
                                               struct ArrowArrayStream* out,
                                               struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->GetStatistics(catalog, db_schema, table_name, approximate == 1, out,
                               error);
}

AdbcStatusCode NetezzaConnectionGetStatisticNames(struct AdbcConnection* connection,
                                                   struct ArrowArrayStream* out,
                                                   struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->GetStatisticNames(out, error);
}

AdbcStatusCode NetezzaConnectionGetTableSchema(
    struct AdbcConnection* connection, const char* catalog, const char* db_schema,
    const char* table_name, struct ArrowSchema* schema, struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->GetTableSchema(catalog, db_schema, table_name, schema, error);
}

AdbcStatusCode NetezzaConnectionGetTableTypes(struct AdbcConnection* connection,
                                               struct ArrowArrayStream* stream,
                                               struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->GetTableTypes(connection, stream, error);
}

AdbcStatusCode NetezzaConnectionInit(struct AdbcConnection* connection,
                                      struct AdbcDatabase* database,
                                      struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->Init(database, error);
}

AdbcStatusCode NetezzaConnectionNew(struct AdbcConnection* connection,
                                     struct AdbcError* error) {
  auto impl = std::make_shared<NetezzaConnection>();
  connection->private_data = new std::shared_ptr<NetezzaConnection>(impl);
  return ADBC_STATUS_OK;
}

AdbcStatusCode NetezzaConnectionReadPartition(struct AdbcConnection* connection,
                                               const uint8_t* serialized_partition,
                                               size_t serialized_length,
                                               struct ArrowArrayStream* out,
                                               struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  return ADBC_STATUS_NOT_IMPLEMENTED;
}

AdbcStatusCode NetezzaConnectionRelease(struct AdbcConnection* connection,
                                         struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  AdbcStatusCode status = (*ptr)->Release(error);
  delete ptr;
  connection->private_data = nullptr;
  return status;
}

AdbcStatusCode NetezzaConnectionRollback(struct AdbcConnection* connection,
                                          struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->Rollback(error);
}

AdbcStatusCode NetezzaConnectionSetOption(struct AdbcConnection* connection,
                                           const char* key, const char* value,
                                           struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->SetOption(key, value, error);
}

AdbcStatusCode NetezzaConnectionSetOptionBytes(struct AdbcConnection* connection,
                                                const char* key, const uint8_t* value,
                                                size_t length, struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->SetOptionBytes(key, value, length, error);
}

AdbcStatusCode NetezzaConnectionSetOptionDouble(struct AdbcConnection* connection,
                                                 const char* key, double value,
                                                 struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->SetOptionDouble(key, value, error);
}

AdbcStatusCode NetezzaConnectionSetOptionInt(struct AdbcConnection* connection,
                                              const char* key, int64_t value,
                                              struct AdbcError* error) {
  if (!connection->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaConnection>*>(connection->private_data);
  return (*ptr)->SetOptionInt(key, value, error);
}

}  // namespace

AdbcStatusCode AdbcConnectionCancel(struct AdbcConnection* connection,
                                    struct AdbcError* error) {
  return NetezzaConnectionCancel(connection, error);
}

AdbcStatusCode AdbcConnectionCommit(struct AdbcConnection* connection,
                                    struct AdbcError* error) {
  return NetezzaConnectionCommit(connection, error);
}

AdbcStatusCode AdbcConnectionGetInfo(struct AdbcConnection* connection,
                                     const uint32_t* info_codes, size_t info_codes_length,
                                     struct ArrowArrayStream* stream,
                                     struct AdbcError* error) {
  return NetezzaConnectionGetInfo(connection, info_codes, info_codes_length, stream,
                                   error);
}

AdbcStatusCode AdbcConnectionGetObjects(struct AdbcConnection* connection, int depth,
                                        const char* catalog, const char* db_schema,
                                        const char* table_name, const char** table_types,
                                        const char* column_name,
                                        struct ArrowArrayStream* stream,
                                        struct AdbcError* error) {
  return NetezzaConnectionGetObjects(connection, depth, catalog, db_schema, table_name,
                                      table_types, column_name, stream, error);
}

AdbcStatusCode AdbcConnectionGetOption(struct AdbcConnection* connection, const char* key,
                                       char* value, size_t* length,
                                       struct AdbcError* error) {
  return NetezzaConnectionGetOption(connection, key, value, length, error);
}

AdbcStatusCode AdbcConnectionGetOptionBytes(struct AdbcConnection* connection,
                                            const char* key, uint8_t* value,
                                            size_t* length, struct AdbcError* error) {
  return NetezzaConnectionGetOptionBytes(connection, key, value, length, error);
}

AdbcStatusCode AdbcConnectionGetOptionInt(struct AdbcConnection* connection,
                                          const char* key, int64_t* value,
                                          struct AdbcError* error) {
  return NetezzaConnectionGetOptionInt(connection, key, value, error);
}

AdbcStatusCode AdbcConnectionGetOptionDouble(struct AdbcConnection* connection,
                                             const char* key, double* value,
                                             struct AdbcError* error) {
  return NetezzaConnectionGetOptionDouble(connection, key, value, error);
}

AdbcStatusCode AdbcConnectionGetStatistics(struct AdbcConnection* connection,
                                           const char* catalog, const char* db_schema,
                                           const char* table_name, char approximate,
                                           struct ArrowArrayStream* out,
                                           struct AdbcError* error) {
  return NetezzaConnectionGetStatistics(connection, catalog, db_schema, table_name,
                                         approximate, out, error);
}

AdbcStatusCode AdbcConnectionGetStatisticNames(struct AdbcConnection* connection,
                                               struct ArrowArrayStream* out,
                                               struct AdbcError* error) {
  return NetezzaConnectionGetStatisticNames(connection, out, error);
}

AdbcStatusCode AdbcConnectionGetTableSchema(struct AdbcConnection* connection,
                                            const char* catalog, const char* db_schema,
                                            const char* table_name,
                                            struct ArrowSchema* schema,
                                            struct AdbcError* error) {
  return NetezzaConnectionGetTableSchema(connection, catalog, db_schema, table_name,
                                          schema, error);
}

AdbcStatusCode AdbcConnectionGetTableTypes(struct AdbcConnection* connection,
                                           struct ArrowArrayStream* stream,
                                           struct AdbcError* error) {
  return NetezzaConnectionGetTableTypes(connection, stream, error);
}

AdbcStatusCode AdbcConnectionInit(struct AdbcConnection* connection,
                                  struct AdbcDatabase* database,
                                  struct AdbcError* error) {
  return NetezzaConnectionInit(connection, database, error);
}

AdbcStatusCode AdbcConnectionNew(struct AdbcConnection* connection,
                                 struct AdbcError* error) {
  return NetezzaConnectionNew(connection, error);
}

AdbcStatusCode AdbcConnectionReadPartition(struct AdbcConnection* connection,
                                           const uint8_t* serialized_partition,
                                           size_t serialized_length,
                                           struct ArrowArrayStream* out,
                                           struct AdbcError* error) {
  return NetezzaConnectionReadPartition(connection, serialized_partition,
                                         serialized_length, out, error);
}

AdbcStatusCode AdbcConnectionRelease(struct AdbcConnection* connection,
                                     struct AdbcError* error) {
  return NetezzaConnectionRelease(connection, error);
}

AdbcStatusCode AdbcConnectionRollback(struct AdbcConnection* connection,
                                      struct AdbcError* error) {
  return NetezzaConnectionRollback(connection, error);
}

AdbcStatusCode AdbcConnectionSetOption(struct AdbcConnection* connection, const char* key,
                                       const char* value, struct AdbcError* error) {
  return NetezzaConnectionSetOption(connection, key, value, error);
}

AdbcStatusCode AdbcConnectionSetOptionBytes(struct AdbcConnection* connection,
                                            const char* key, const uint8_t* value,
                                            size_t length, struct AdbcError* error) {
  return NetezzaConnectionSetOptionBytes(connection, key, value, length, error);
}

AdbcStatusCode AdbcConnectionSetOptionInt(struct AdbcConnection* connection,
                                          const char* key, int64_t value,
                                          struct AdbcError* error) {
  return NetezzaConnectionSetOptionInt(connection, key, value, error);
}

AdbcStatusCode AdbcConnectionSetOptionDouble(struct AdbcConnection* connection,
                                             const char* key, double value,
                                             struct AdbcError* error) {
  return NetezzaConnectionSetOptionDouble(connection, key, value, error);
}

// ---------------------------------------------------------------------
// AdbcStatement

namespace {
AdbcStatusCode NetezzaStatementBind(struct AdbcStatement* statement,
                                     struct ArrowArray* values,
                                     struct ArrowSchema* schema,
                                     struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto* ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->Bind(values, schema, error);
}

AdbcStatusCode NetezzaStatementBindStream(struct AdbcStatement* statement,
                                           struct ArrowArrayStream* stream,
                                           struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto* ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->Bind(stream, error);
}

AdbcStatusCode NetezzaStatementCancel(struct AdbcStatement* statement,
                                       struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto* ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->Cancel(error);
}

AdbcStatusCode NetezzaStatementExecutePartitions(struct AdbcStatement* statement,
                                                  struct ArrowSchema* schema,
                                                  struct AdbcPartitions* partitions,
                                                  int64_t* rows_affected,
                                                  struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  return ADBC_STATUS_NOT_IMPLEMENTED;
}

AdbcStatusCode NetezzaStatementExecuteQuery(struct AdbcStatement* statement,
                                             struct ArrowArrayStream* output,
                                             int64_t* rows_affected,
                                             struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto* ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->ExecuteQuery(output, rows_affected, error);
}

AdbcStatusCode NetezzaStatementExecuteSchema(struct AdbcStatement* statement,
                                              struct ArrowSchema* schema,
                                              struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto* ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->ExecuteSchema(schema, error);
}

AdbcStatusCode NetezzaStatementGetOption(struct AdbcStatement* statement,
                                          const char* key, char* value, size_t* length,
                                          struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->GetOption(key, value, length, error);
}

AdbcStatusCode NetezzaStatementGetOptionBytes(struct AdbcStatement* statement,
                                               const char* key, uint8_t* value,
                                               size_t* length, struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->GetOptionBytes(key, value, length, error);
}

AdbcStatusCode NetezzaStatementGetOptionDouble(struct AdbcStatement* statement,
                                                const char* key, double* value,
                                                struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->GetOptionDouble(key, value, error);
}

AdbcStatusCode NetezzaStatementGetOptionInt(struct AdbcStatement* statement,
                                             const char* key, int64_t* value,
                                             struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->GetOptionInt(key, value, error);
}

AdbcStatusCode NetezzaStatementGetParameterSchema(struct AdbcStatement* statement,
                                                   struct ArrowSchema* schema,
                                                   struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto* ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->GetParameterSchema(schema, error);
}

AdbcStatusCode NetezzaStatementNew(struct AdbcConnection* connection,
                                    struct AdbcStatement* statement,
                                    struct AdbcError* error) {
  auto impl = std::make_shared<NetezzaStatement>();
  statement->private_data = new std::shared_ptr<NetezzaStatement>(impl);
  return impl->New(connection, error);
}

AdbcStatusCode NetezzaStatementPrepare(struct AdbcStatement* statement,
                                        struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto* ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->Prepare(error);
}

AdbcStatusCode NetezzaStatementRelease(struct AdbcStatement* statement,
                                        struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto* ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  auto status = (*ptr)->Release(error);
  delete ptr;
  statement->private_data = nullptr;
  return status;
}

AdbcStatusCode NetezzaStatementSetOption(struct AdbcStatement* statement,
                                          const char* key, const char* value,
                                          struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto* ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->SetOption(key, value, error);
}

AdbcStatusCode NetezzaStatementSetOptionBytes(struct AdbcStatement* statement,
                                               const char* key, const uint8_t* value,
                                               size_t length, struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->SetOptionBytes(key, value, length, error);
}

AdbcStatusCode NetezzaStatementSetOptionDouble(struct AdbcStatement* statement,
                                                const char* key, double value,
                                                struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->SetOptionDouble(key, value, error);
}

AdbcStatusCode NetezzaStatementSetOptionInt(struct AdbcStatement* statement,
                                             const char* key, int64_t value,
                                             struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->SetOptionInt(key, value, error);
}

AdbcStatusCode NetezzaStatementSetSqlQuery(struct AdbcStatement* statement,
                                            const char* query, struct AdbcError* error) {
  if (!statement->private_data) return ADBC_STATUS_INVALID_STATE;
  auto* ptr =
      reinterpret_cast<std::shared_ptr<NetezzaStatement>*>(statement->private_data);
  return (*ptr)->SetSqlQuery(query, error);
}
}  // namespace

AdbcStatusCode AdbcStatementBind(struct AdbcStatement* statement,
                                 struct ArrowArray* values, struct ArrowSchema* schema,
                                 struct AdbcError* error) {
  return NetezzaStatementBind(statement, values, schema, error);
}

AdbcStatusCode AdbcStatementBindStream(struct AdbcStatement* statement,
                                       struct ArrowArrayStream* stream,
                                       struct AdbcError* error) {
  return NetezzaStatementBindStream(statement, stream, error);
}

AdbcStatusCode AdbcStatementCancel(struct AdbcStatement* statement,
                                   struct AdbcError* error) {
  return NetezzaStatementCancel(statement, error);
}

AdbcStatusCode AdbcStatementExecutePartitions(struct AdbcStatement* statement,
                                              ArrowSchema* schema,
                                              struct AdbcPartitions* partitions,
                                              int64_t* rows_affected,
                                              struct AdbcError* error) {
  return NetezzaStatementExecutePartitions(statement, schema, partitions, rows_affected,
                                            error);
}

AdbcStatusCode AdbcStatementExecuteQuery(struct AdbcStatement* statement,
                                         struct ArrowArrayStream* output,
                                         int64_t* rows_affected,
                                         struct AdbcError* error) {
  return NetezzaStatementExecuteQuery(statement, output, rows_affected, error);
}

AdbcStatusCode AdbcStatementExecuteSchema(struct AdbcStatement* statement,
                                          ArrowSchema* schema, struct AdbcError* error) {
  return NetezzaStatementExecuteSchema(statement, schema, error);
}

AdbcStatusCode AdbcStatementGetOption(struct AdbcStatement* statement, const char* key,
                                      char* value, size_t* length,
                                      struct AdbcError* error) {
  return NetezzaStatementGetOption(statement, key, value, length, error);
}

AdbcStatusCode AdbcStatementGetOptionBytes(struct AdbcStatement* statement,
                                           const char* key, uint8_t* value,
                                           size_t* length, struct AdbcError* error) {
  return NetezzaStatementGetOptionBytes(statement, key, value, length, error);
}

AdbcStatusCode AdbcStatementGetOptionInt(struct AdbcStatement* statement, const char* key,
                                         int64_t* value, struct AdbcError* error) {
  return NetezzaStatementGetOptionInt(statement, key, value, error);
}

AdbcStatusCode AdbcStatementGetOptionDouble(struct AdbcStatement* statement,
                                            const char* key, double* value,
                                            struct AdbcError* error) {
  return NetezzaStatementGetOptionDouble(statement, key, value, error);
}

AdbcStatusCode AdbcStatementGetParameterSchema(struct AdbcStatement* statement,
                                               struct ArrowSchema* schema,
                                               struct AdbcError* error) {
  return NetezzaStatementGetParameterSchema(statement, schema, error);
}

AdbcStatusCode AdbcStatementNew(struct AdbcConnection* connection,
                                struct AdbcStatement* statement,
                                struct AdbcError* error) {
  return NetezzaStatementNew(connection, statement, error);
}

AdbcStatusCode AdbcStatementPrepare(struct AdbcStatement* statement,
                                    struct AdbcError* error) {
  return NetezzaStatementPrepare(statement, error);
}

AdbcStatusCode AdbcStatementRelease(struct AdbcStatement* statement,
                                    struct AdbcError* error) {
  return NetezzaStatementRelease(statement, error);
}

AdbcStatusCode AdbcStatementSetOption(struct AdbcStatement* statement, const char* key,
                                      const char* value, struct AdbcError* error) {
  return NetezzaStatementSetOption(statement, key, value, error);
}

AdbcStatusCode AdbcStatementSetOptionBytes(struct AdbcStatement* statement,
                                           const char* key, const uint8_t* value,
                                           size_t length, struct AdbcError* error) {
  return NetezzaStatementSetOptionBytes(statement, key, value, length, error);
}

AdbcStatusCode AdbcStatementSetOptionInt(struct AdbcStatement* statement, const char* key,
                                         int64_t value, struct AdbcError* error) {
  return NetezzaStatementSetOptionInt(statement, key, value, error);
}

AdbcStatusCode AdbcStatementSetOptionDouble(struct AdbcStatement* statement,
                                            const char* key, double value,
                                            struct AdbcError* error) {
  return NetezzaStatementSetOptionDouble(statement, key, value, error);
}

AdbcStatusCode AdbcStatementSetSqlQuery(struct AdbcStatement* statement,
                                        const char* query, struct AdbcError* error) {
  return NetezzaStatementSetSqlQuery(statement, query, error);
}

extern "C" {
ADBC_EXPORT
AdbcStatusCode NetezzaDriverInit(int version, void* raw_driver,
                                    struct AdbcError* error) {
  if (version != ADBC_VERSION_1_0_0 && version != ADBC_VERSION_1_1_0) {
    return ADBC_STATUS_NOT_IMPLEMENTED;
  }
  if (!raw_driver) return ADBC_STATUS_INVALID_ARGUMENT;

  auto* driver = reinterpret_cast<struct AdbcDriver*>(raw_driver);
  if (version >= ADBC_VERSION_1_1_0) {
    std::memset(driver, 0, ADBC_DRIVER_1_1_0_SIZE);

    driver->ErrorGetDetailCount = CommonErrorGetDetailCount;
    driver->ErrorGetDetail = CommonErrorGetDetail;
    driver->ErrorFromArrayStream = NetezzaErrorFromArrayStream;

    driver->DatabaseGetOption = NetezzaDatabaseGetOption;
    driver->DatabaseGetOptionBytes = NetezzaDatabaseGetOptionBytes;
    driver->DatabaseGetOptionDouble = NetezzaDatabaseGetOptionDouble;
    driver->DatabaseGetOptionInt = NetezzaDatabaseGetOptionInt;
    driver->DatabaseSetOptionBytes = NetezzaDatabaseSetOptionBytes;
    driver->DatabaseSetOptionDouble = NetezzaDatabaseSetOptionDouble;
    driver->DatabaseSetOptionInt = NetezzaDatabaseSetOptionInt;

    driver->ConnectionCancel = NetezzaConnectionCancel;
    driver->ConnectionGetOption = NetezzaConnectionGetOption;
    driver->ConnectionGetOptionBytes = NetezzaConnectionGetOptionBytes;
    driver->ConnectionGetOptionDouble = NetezzaConnectionGetOptionDouble;
    driver->ConnectionGetOptionInt = NetezzaConnectionGetOptionInt;
    driver->ConnectionGetStatistics = NetezzaConnectionGetStatistics;
    driver->ConnectionGetStatisticNames = NetezzaConnectionGetStatisticNames;
    driver->ConnectionSetOptionBytes = NetezzaConnectionSetOptionBytes;
    driver->ConnectionSetOptionDouble = NetezzaConnectionSetOptionDouble;
    driver->ConnectionSetOptionInt = NetezzaConnectionSetOptionInt;

    driver->StatementCancel = NetezzaStatementCancel;
    driver->StatementExecuteSchema = NetezzaStatementExecuteSchema;
    driver->StatementGetOption = NetezzaStatementGetOption;
    driver->StatementGetOptionBytes = NetezzaStatementGetOptionBytes;
    driver->StatementGetOptionDouble = NetezzaStatementGetOptionDouble;
    driver->StatementGetOptionInt = NetezzaStatementGetOptionInt;
    driver->StatementSetOptionBytes = NetezzaStatementSetOptionBytes;
    driver->StatementSetOptionDouble = NetezzaStatementSetOptionDouble;
    driver->StatementSetOptionInt = NetezzaStatementSetOptionInt;
  } else {
    std::memset(driver, 0, ADBC_DRIVER_1_0_0_SIZE);
  }

  driver->DatabaseInit = NetezzaDatabaseInit;
  driver->DatabaseNew = NetezzaDatabaseNew;
  driver->DatabaseRelease = NetezzaDatabaseRelease;
  driver->DatabaseSetOption = NetezzaDatabaseSetOption;

  driver->ConnectionCommit = NetezzaConnectionCommit;
  driver->ConnectionGetInfo = NetezzaConnectionGetInfo;
  driver->ConnectionGetObjects = NetezzaConnectionGetObjects;
  driver->ConnectionGetTableSchema = NetezzaConnectionGetTableSchema;
  driver->ConnectionGetTableTypes = NetezzaConnectionGetTableTypes;
  driver->ConnectionInit = NetezzaConnectionInit;
  driver->ConnectionNew = NetezzaConnectionNew;
  driver->ConnectionReadPartition = NetezzaConnectionReadPartition;
  driver->ConnectionRelease = NetezzaConnectionRelease;
  driver->ConnectionRollback = NetezzaConnectionRollback;
  driver->ConnectionSetOption = NetezzaConnectionSetOption;

  driver->StatementBind = NetezzaStatementBind;
  driver->StatementBindStream = NetezzaStatementBindStream;
  driver->StatementExecutePartitions = NetezzaStatementExecutePartitions;
  driver->StatementExecuteQuery = NetezzaStatementExecuteQuery;
  driver->StatementGetParameterSchema = NetezzaStatementGetParameterSchema;
  driver->StatementNew = NetezzaStatementNew;
  driver->StatementPrepare = NetezzaStatementPrepare;
  driver->StatementRelease = NetezzaStatementRelease;
  driver->StatementSetOption = NetezzaStatementSetOption;
  driver->StatementSetSqlQuery = NetezzaStatementSetSqlQuery;

  return ADBC_STATUS_OK;
}

ADBC_EXPORT
AdbcStatusCode AdbcDriverInit(int version, void* raw_driver, struct AdbcError* error) {
  return NetezzaDriverInit(version, raw_driver, error);
}
}
