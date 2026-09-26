"""Exercise the durable ContentCatalog pending-job SQL.

The C++ worker intentionally hashes files asynchronously. These tests verify the
SQLite contract that makes that queue crash-safe: child aliases cascade with the
job, a stale worker generation cannot acknowledge a newer submission, and the
current generation can.

Run:
    python scripts/test_content_catalog_pending_sql.py
"""

import pathlib
import re
import sqlite3
import unittest


SOURCE = (
    pathlib.Path(__file__).resolve().parents[1]
    / "OpenNet/Core/Content/SqliteContentCatalog.cpp"
).read_text(encoding="utf-8-sig")


def raw_sql(section):
    match = re.search(r'R"\((.*?)\)"', section, re.S)
    if not match:
        raise RuntimeError("Expected raw SQL block was not found.")
    return match.group(1)


CREATE_TABLES = SOURCE.split(
    "void SqliteContentCatalog::CreateTables()", 1
)[1].split(
    "std::string SqliteContentCatalog::PathToUtf8", 1
)[0]

SCHEMA_SQL = raw_sql(CREATE_TABLES)

COMPLETE_METHOD = SOURCE.split(
    "SqliteContentCatalog::CompletePendingJob(", 1
)[1].split(
    "void SqliteContentCatalog::Upsert(", 1
)[0]

ACK_MATCH = re.search(
    r'"DELETE FROM content_pending_job "\s*'
    r'"WHERE local_path=\? AND generation=\?;"',
    COMPLETE_METHOD,
)
if not ACK_MATCH:
    raise RuntimeError(
        "CompletePendingJob no longer uses the generation-guarded ACK."
    )
ACK_SQL = (
    "DELETE FROM content_pending_job "
    "WHERE local_path=? AND generation=?;"
)


class ContentCatalogPendingSqlTests(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(":memory:")
        self.addCleanup(self.db.close)
        self.db.execute("PRAGMA foreign_keys = ON")
        self.db.executescript(SCHEMA_SQL)

    def insert_job(self, generation=1):
        self.db.execute(
            """
            INSERT INTO content_pending_job(
                local_path,
                generation,
                source_kind,
                source_id,
                source_file_index,
                file_size,
                last_write_ticks
            )
            VALUES(
                'C:/Downloads/file.bin',
                ?,
                2,
                'http-record',
                NULL,
                1234,
                5678
            )
            """,
            (generation,),
        )

    def insert_aliases(self):
        self.db.execute(
            """
            INSERT INTO content_pending_identity(
                local_path, algorithm, digest
            )
            VALUES('C:/Downloads/file.bin', 3, ?)
            """,
            (bytes(range(32)),),
        )
        self.db.execute(
            """
            INSERT INTO content_pending_resource_key(
                local_path, algorithm, digest
            )
            VALUES('C:/Downloads/file.bin', 1, ?)
            """,
            (bytes(reversed(range(32))),),
        )

    def test_pending_children_require_parent_and_are_unique(self):
        self.insert_job()
        self.insert_aliases()

        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(
                """
                INSERT INTO content_pending_identity(
                    local_path, algorithm, digest
                )
                VALUES('C:/Downloads/file.bin', 3, ?)
                """,
                (bytes(range(32)),),
            )

        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(
                """
                INSERT INTO content_pending_resource_key(
                    local_path, algorithm, digest
                )
                VALUES('C:/Missing/file.bin', 1, ?)
                """,
                (bytes(range(32)),),
            )

    def test_stale_generation_ack_cannot_delete_newer_job(self):
        self.insert_job(generation=2)
        self.insert_aliases()

        self.db.execute(
            ACK_SQL,
            ("C:/Downloads/file.bin", 1),
        )

        self.assertEqual(
            self.db.execute(
                """
                SELECT generation
                FROM content_pending_job
                WHERE local_path='C:/Downloads/file.bin'
                """
            ).fetchone(),
            (2,),
        )
        self.assertEqual(
            self.db.execute(
                "SELECT COUNT(*) FROM content_pending_identity"
            ).fetchone(),
            (1,),
        )
        self.assertEqual(
            self.db.execute(
                "SELECT COUNT(*) FROM content_pending_resource_key"
            ).fetchone(),
            (1,),
        )

    def test_current_generation_ack_cascades_children(self):
        self.insert_job(generation=7)
        self.insert_aliases()

        self.db.execute(
            ACK_SQL,
            ("C:/Downloads/file.bin", 7),
        )

        self.assertEqual(
            self.db.execute(
                "SELECT COUNT(*) FROM content_pending_job"
            ).fetchone(),
            (0,),
        )
        self.assertEqual(
            self.db.execute(
                "SELECT COUNT(*) FROM content_pending_identity"
            ).fetchone(),
            (0,),
        )
        self.assertEqual(
            self.db.execute(
                "SELECT COUNT(*) FROM content_pending_resource_key"
            ).fetchone(),
            (0,),
        )

    def test_replacing_parent_generation_preserves_children_until_ack(self):
        self.insert_job(generation=1)
        self.insert_aliases()

        self.db.execute(
            """
            UPDATE content_pending_job
            SET generation=2
            WHERE local_path='C:/Downloads/file.bin'
            """
        )

        self.assertEqual(
            self.db.execute(
                "SELECT COUNT(*) FROM content_pending_identity"
            ).fetchone(),
            (1,),
        )
        self.assertEqual(
            self.db.execute(
                "SELECT COUNT(*) FROM content_pending_resource_key"
            ).fetchone(),
            (1,),
        )

        self.db.execute(
            ACK_SQL,
            ("C:/Downloads/file.bin", 2),
        )
        self.assertEqual(
            self.db.execute(
                "SELECT COUNT(*) FROM content_pending_identity"
            ).fetchone(),
            (0,),
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
