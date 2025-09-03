# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

from pathlib import Path
from typing import Generator
import pyarrow as pa

import pyarrow
import pyarrow.dataset
import pytest

from adbc_driver_netezza import StatementOptions, dbapi
import adbc_driver_manager
import adbc_driver_netezza.dbapi


@pytest.fixture
def netezza(netezza_uri: str) -> Generator[dbapi.Connection, None, None]:
    with dbapi.connect(netezza_uri) as conn:
        yield conn


def test_conn_current_catalog(netezza: dbapi.Connection) -> None:
    assert netezza.adbc_current_catalog != ""


def test_conn_current_db_schema(netezza: dbapi.Connection) -> None:
    assert netezza.adbc_current_db_schema == "ADMIN"


@pytest.mark.skip(reason="Not relevant for netezza")
def test_conn_change_db_schema(netezza: dbapi.Connection) -> None:
    assert netezza.adbc_current_db_schema == "ADMIN"

    with netezza.cursor() as cur:
        cur.execute("CREATE SCHEMA  dbapischema")

    assert netezza.adbc_current_db_schema == "ADMIN"
    netezza.adbc_current_db_schema = "dbapischema"
    assert netezza.adbc_current_db_schema == "dbapischema"

@pytest.mark.skip(reason="Not relevant for netezza")
def test_conn_get_info(netezza: dbapi.Connection) -> None:
    info = netezza.adbc_get_info()
    assert info["driver_name"] == "ADBC Netezza Driver"
    assert info["driver_adbc_version"] == 1_001_000
    assert info["vendor_name"] == "Netezza"


def test_query_batch_size(netezza: dbapi.Connection):
    with netezza.cursor() as cur:
        int_array = pa.array(range(1, 65537), type=pa.int32())
        table = pa.table({"ints": int_array})
        cur.adbc_ingest("test_batch_size", table, mode="create")

        cur.execute("SELECT * FROM test_batch_size")
        table = cur.fetch_arrow_table()
        assert len(table.to_batches()) == 1

        cur.adbc_statement.set_options(
            **{StatementOptions.BATCH_SIZE_HINT_BYTES.value: "1"}
        )
        assert (
            cur.adbc_statement.get_option_int(
                StatementOptions.BATCH_SIZE_HINT_BYTES.value
            )
            == 1
        )
        cur.execute("SELECT * FROM test_batch_size")
        table = cur.fetch_arrow_table()
        assert len(table.to_batches()[0]) == 65536

        cur.adbc_statement.set_options(
            **{StatementOptions.BATCH_SIZE_HINT_BYTES.value: "4096"}
        )
        assert (
            cur.adbc_statement.get_option_int(
                StatementOptions.BATCH_SIZE_HINT_BYTES.value
            )
            == 4096
        )
        cur.execute("SELECT * FROM test_batch_size")
        table = cur.fetch_arrow_table()
        assert len(table.to_batches()) >= 1


@pytest.mark.skip(reason="Not relevant for netezza")
def test_query_cancel(netezza: dbapi.Connection) -> None:
    with netezza.cursor() as cur:
        int_array = pa.array(range(0, 1048576), type=pa.int32())
        table = pa.table({"ints": int_array})
        cur.adbc_ingest("test_batch_size", table, mode="replace")
        netezza.commit()

    # Ensure different ways of reading all raise the desired error
    with netezza.cursor() as cur:
        cur.execute("SELECT * FROM test_batch_size")
        cur.adbc_cancel()
        with pytest.raises(netezza.OperationalError, match="canceling statement"):
            cur.fetchone()

    netezza.rollback()

    with netezza.cursor() as cur:
        cur.execute("SELECT * FROM test_batch_size")
        cur.adbc_cancel()
        with pytest.raises(netezza.OperationalError, match="canceling statement"):
            cur.fetch_arrow_table()

    netezza.rollback()

    with netezza.cursor() as cur:
        cur.execute("SELECT * FROM test_batch_size")
        cur.adbc_cancel()
        with pytest.raises(netezza.OperationalError, match="canceling statement"):
            cur.fetch_df()


def test_query_execute_schema(netezza: dbapi.Connection) -> None:
    with netezza.cursor() as cur:
        schema = cur.adbc_execute_schema("SELECT 1 AS foo")
        assert schema == pyarrow.schema([("FOO", "int32")])


def test_query_invalid(netezza: dbapi.Connection) -> None:
    with netezza.cursor() as cur:
        with pytest.raises(
            netezza.ProgrammingError, match="failed to prepare query"
        ) as excinfo:
            cur.execute("SELECT * FROM tabledoesnotexist")

        assert excinfo.value.sqlstate == "PGRES"
        assert len(excinfo.value.details) > 0


def test_query_trivial(netezza: dbapi.Connection):
    with netezza.cursor() as cur:
        cur.execute("SELECT 1")
        result = cur.fetchone()
        assert result == (1,)


def test_stmt_ingest(netezza: dbapi.Connection) -> None:
    table = pyarrow.table(
        [
            [1, 2, 3],
            ["a", "c", "b"],
        ],
        names=["INTS", "STRS"],
    )
    double_table = pyarrow.table(
        [
            [1, 1, 2, 2, 3, 3],
            ["a", "a", "c", "c", "b", "b"],
        ],
        names=["INTS", "STRS"],
    )

    reader_et_options = {"delim" : "','", "MaxErrors":0, "SkipRows":1, "QuotedValue": "Double"}
    with netezza.cursor() as cur:
        cur.execute("DROP TABLE test_ingest IF EXISTS ")

        with pytest.raises(
            adbc_driver_manager.ProgrammingError, match='relation does not exist'
        ):
            cur.adbc_ingest("test_ingest", table, mode="append")

        netezza.rollback()

        cur.adbc_ingest("test_ingest", table, mode="replace", reader_et_options=reader_et_options)
        cur.execute("SELECT * FROM test_ingest ORDER BY ints")
        assert cur.fetch_arrow_table() == table

        with pytest.raises(
            netezza.ProgrammingError, match='"TEST_INGEST" already exists'
        ):
            cur.adbc_ingest("test_ingest", table, mode="create", reader_et_options=reader_et_options)

        reader_et_options = {"delim" : "','", "MaxErrors":0, "SkipRows":1, "QuotedValue": "Double"}
        cur.adbc_ingest("TEST_INGEST", table, mode="create_append", reader_et_options=reader_et_options)
        cur.execute("SELECT * FROM test_ingest ORDER BY ints")
        assert cur.fetch_arrow_table() == double_table

        cur.adbc_ingest("TEST_INGEST", table, mode="replace", reader_et_options=reader_et_options)
        cur.execute("SELECT * FROM TEST_INGEST ORDER BY ints")
        result = cur.fetch_arrow_table()
        assert result == table

        cur.execute("DROP TABLE TEST_INGEST IF EXISTS ")
        cur.adbc_ingest("TEST_INGEST", table, mode="create_append", reader_et_options=reader_et_options)
        cur.execute("SELECT * FROM TEST_INGEST ORDER BY ints")
        result = cur.fetch_arrow_table()
        assert result == table

        cur.execute("DROP TABLE TEST_INGEST IF EXISTS ")

        cur.adbc_ingest("TEST_INGEST", table, mode="create", reader_et_options=reader_et_options)
        cur.execute("SELECT * FROM TEST_INGEST ORDER BY ints")
        result = cur.fetch_arrow_table()
        assert result == table

@pytest.mark.skip(reason="Not relevant for netezza")
def test_stmt_ingest_dataset(netezza: dbapi.Connection, tmp_path: Path) -> None:
    # Regression test for https://github.com/apache/arrow-adbc/issues/1310
    table = pyarrow.table(
        [
            [1, 1, 2, 2, 3, 3],
            ["a", "a", None, None, "b", "b"],
        ],
        schema=pyarrow.schema([("ints", "int32"), ("strs", "string")]),
    )
    pyarrow.dataset.write_dataset(
        table, tmp_path, format="parquet", partitioning=["ints"]
    )
    ds = pyarrow.dataset.dataset(tmp_path, format="parquet", partitioning=["ints"])

    with netezza.cursor() as cur:
        for item in (
            lambda: ds,
            lambda: ds.scanner(),
            lambda: ds.scanner().to_reader(),
            lambda: ds.scanner().to_table(),
        ):
            cur.execute("DROP TABLE IF EXISTS test_ingest")

            cur.adbc_ingest(
                "test_ingest",
                item(),
                mode="create_append",
            )
            cur.execute("SELECT ints, strs FROM test_ingest ORDER BY ints")
            assert cur.fetch_arrow_table() == table

@pytest.mark.skip(reason="Not relevant for netezza")
def test_stmt_ingest_multi(netezza: dbapi.Connection) -> None:
    # Regression test for https://github.com/apache/arrow-adbc/issues/1310
    table = pyarrow.table(
        [
            [1, 1, 2, 2, 3, 3],
            ["a", "a", None, None, "b", "b"],
        ],
        names=["ints", "strs"],
    )

    with netezza.cursor() as cur:
        cur.execute("DROP TABLE test_ingest IF EXISTS ")

        cur.adbc_ingest(
            "test_ingest",
            table.to_batches(max_chunksize=2),
            mode="create_append",
        )
        cur.execute("SELECT * FROM test_ingest ORDER BY ints")
        assert cur.fetch_arrow_table() == table


def test_ddl(netezza: dbapi.Connection):
    with netezza.cursor() as cur:
        cur.execute("DROP TABLE test_ddl IF EXISTS")
        assert cur.fetchone() is None

        cur.execute("CREATE TABLE test_ddl (ints INT)")
        assert cur.fetchone() is None

        cur.execute("INSERT INTO test_ddl VALUES (1)")

        cur.execute("SELECT * FROM test_ddl")
        assert cur.fetchone() == (1,)


def test_crash(netezza: dbapi.Connection) -> None:
    with netezza.cursor() as cur:
        cur.execute("SELECT 1")
        result = cur.fetchone()
        assert result == (1,)


def test_reuse(netezza: dbapi.Connection) -> None:
    with netezza.cursor() as cur:
        cur.execute("DROP TABLE test_batch_size IF EXISTS")

        int_array = pa.array(range(1, 65537), type=pa.int32())
        table = pa.table({"ints": int_array})
        cur.adbc_ingest("test_batch_size", table, mode="create")

        cur.execute("SELECT * FROM test_batch_size ORDER BY ints ASC")
        assert cur.fetchone() == (1,)

        cur.execute("SELECT 1")
        assert cur.fetchone() == (1,)

        cur.execute("SELECT 2")
        assert cur.fetchone() == (2,)


def test_ingest_schema(netezza: dbapi.Connection) -> None:
    table = pyarrow.Table.from_pydict({"NUMBERS": [1, 2], "LETTERS": ["a", "b"]})

    with netezza.cursor() as cur:
        cur.execute("CREATE SCHEMA TESTSCHEMA")
        cur.execute("DROP TABLE testschema.foo IF EXISTS ")

        netezza.commit()

        reader_et_options = {"delim" : "','", "MaxErrors":0, "SkipRows":1, "QuotedValue": "Double"}

        cur.adbc_ingest("foo", table, mode="create", db_schema_name="testschema", reader_et_options=reader_et_options)

        cur.execute("SELECT * FROM TESTSCHEMA.foo ORDER BY numbers")
        result = cur.fetch_arrow_table()
        assert result == table
        cur.execute("DROP SCHEMA TESTSCHEMA CASCADE")
        netezza.commit()

def test_ingest(netezza: dbapi.Connection) -> None:
    table = pyarrow.Table.from_pydict({"NUMBERS": [1, 2], "LETTERS": ["a", "b"]})

    with netezza.cursor() as cur:
        reader_et_options = {"delim" : "','", "MaxErrors":0, "SkipRows":1, "QuotedValue": "Double"}
        table = table.sort_by([(table.schema.get_field_index(k), "ascending") for k in ["NUMBERS", "LETTERS"]])
        cur.adbc_ingest("foo", table, mode="replace", db_schema_name="ADMIN", reader_et_options=reader_et_options)

        cur.execute("SELECT * FROM ADMIN.foo")
        assert cur.fetch_arrow_table() == table

        with pytest.raises(dbapi.NotSupportedError):
            cur.adbc_ingest("foo", table, catalog_name="main")


@pytest.mark.skip(reason="Not implemented on Netezza yet")
def test_ingest_temporary(netezza: dbapi.Connection) -> None:
    table = pyarrow.Table.from_pydict(
        {
            "numbers": [1, 2],
            "letters": ["a", "b"],
        }
    )
    temp = pyarrow.Table.from_pydict(
        {
            "ints": [3, 4],
            "strs": ["c", "d"],
        }
    )

    table2 = pyarrow.Table.from_pydict(
        {
            "numbers": [1, 2, 1, 2],
            "letters": ["a", "b", "a", "b"],
        }
    )
    temp2 = pyarrow.Table.from_pydict(
        {
            "ints": [3, 4, 3, 4],
            "strs": ["c", "d", "c", "d"],
        }
    )

    with netezza.cursor() as cur:
        cur.execute("DROP TABLE ADMIN.temporary if exists")

        cur.adbc_ingest("temporary", table, mode="create")
        cur.adbc_ingest("temporary", temp, mode="create", temporary=True)

        cur.execute("SELECT * FROM public.temporary")
        assert cur.fetch_arrow_table() == table
        cur.execute("SELECT * FROM pg_temp.temporary")
        assert cur.fetch_arrow_table() == temp
        cur.execute("SELECT * FROM temporary")
        assert cur.fetch_arrow_table() == temp

        cur.adbc_ingest("temporary", table, mode="append")
        cur.adbc_ingest("temporary", temp, mode="append", temporary=True)

        cur.execute("SELECT * FROM ADMIN.temporary")
        assert cur.fetch_arrow_table() == table2
        cur.execute("SELECT * FROM pg_temp.temporary")
        assert cur.fetch_arrow_table() == temp2
        cur.execute("SELECT * FROM temporary")
        assert cur.fetch_arrow_table() == temp2

        cur.adbc_ingest("temporary", table, mode="replace")
        cur.adbc_ingest("temporary", temp, mode="replace", temporary=True)

        cur.execute("SELECT * FROM ADMIN.temporary")
        assert cur.fetch_arrow_table() == table
        cur.execute("SELECT * FROM pg_temp.temporary")
        assert cur.fetch_arrow_table() == temp
        cur.execute("SELECT * FROM temporary")
        assert cur.fetch_arrow_table() == temp

        cur.adbc_ingest("temporary", table, mode="create_append")
        cur.adbc_ingest("temporary", temp, mode="create_append", temporary=True)

        cur.execute("SELECT * FROM ADMIN.temporary")
        assert cur.fetch_arrow_table() == table2
        cur.execute("SELECT * FROM pg_temp.temporary")
        assert cur.fetch_arrow_table() == temp2
        cur.execute("SELECT * FROM temporary")
        assert cur.fetch_arrow_table() == temp2
