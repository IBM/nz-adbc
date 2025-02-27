<!---
  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing,
  software distributed under the License is distributed on an
  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
  KIND, either express or implied.  See the License for the
  specific language governing permissions and limitations
  under the License.
-->

# ADBC Netezza Driver

This implements an ADBC driver that wraps [libpq][libpq], the client
library for PostgreSQL.  This is still a work in progress.

This project owes credit to 0x0L's [pgeon][pgeon] for the overall
approach.

**NOTE:** this project is not affiliated with PostgreSQL in any way.

[libpq]: https://www.postgresql.org/docs/current/libpq.html
[pgeon]: https://github.com/0x0L/pgeon

## Building

Dependencies: libpq itself. This can be installed with your favorite
package manager; however, you may need to set the `PKG_CONFIG_PATH`
environment variable such that `pkg-config` can find libpq.

See [CONTRIBUTING.md](../../CONTRIBUTING.md) for details.

## Testing

A running instance of Netezza is required.

Then, to run the tests, set the environment variable specifying the
Netezza URI before running tests:

```shell
$ export ADBC_NETEZZA_TEST_URI=netezza://user:password@host:5480/dbname/schemaname/
$ ctest
```

Users of VSCode can use the CMake extension with the supplied CMakeUserPresets.json
example to supply the required CMake and environment variables required to build and
run tests.
