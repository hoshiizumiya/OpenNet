"""Exercise production HTTP download SQL ownership constraints.

This validates the durable half of OpenNet's one-writer invariant:
active URL/output uniqueness, late output-path claiming, status-based ownership
release, and legacy duplicate cleanup. It intentionally does not test C++
locking or aria2/libtorrent process behavior.

Run:
    python scripts/test_http_persistence_sql.py
"""

import pathlib
import re
import sqlite3
import unittest


SOURCE = (
    pathlib.Path(__file__).resolve().parents[1]
    / "OpenNet/Core/HttpStateManager.cpp"
).read_text(encoding="utf-8-sig")

CREATE_TABLES = SOURCE.split(
    "void HttpStateManager::CreateTables()", 1
)[1].split(
    "// ------------------------------------------------------------------\n"
    "    //  One-time JSON", 1
)[0]

SQL_BLOCKS = re.findall(r'R"\((.*?)\)"', CREATE_TABLES, re.S)

if len(SQL_BLOCKS) < 3:
    raise RuntimeError(
        "HttpStateManager::CreateTables no longer exposes the expected "
        "schema/url-dedupe/output-dedupe SQL blocks."
    )

SCHEMA_SQL = SQL_BLOCKS[0]
ACTIVE_URL_SQL = SQL_BLOCKS[1]
ACTIVE_OUTPUT_SQL = SQL_BLOCKS[2]


class HttpPersistenceOwnershipTests(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(":memory:")
        self.addCleanup(self.db.close)
        self.db.executescript(SCHEMA_SQL)
        self.db.executescript(ACTIVE_URL_SQL)
        self.db.executescript(ACTIVE_OUTPUT_SQL)

    def add(
        self,
        record_id,
        url,
        output_key,
        *,
        status=0,
        completed_size=0,
        added_timestamp=1,
    ):
        self.db.execute(
            """
            INSERT INTO http_downloads (
                record_id,
                url,
                save_path,
                file_name,
                name,
                added_timestamp,
                completed_size,
                status,
                output_key
            )
            VALUES (?, ?, 'C:/Downloads', 'file.bin', 'file.bin', ?, ?, ?, ?)
            """,
            (
                record_id,
                url,
                added_timestamp,
                completed_size,
                status,
                output_key,
            ),
        )

    def test_active_url_is_unique(self):
        self.add("a", "https://example.test/file", "c:/downloads/a.bin")
        with self.assertRaises(sqlite3.IntegrityError):
            self.add(
                "b",
                "https://example.test/file",
                "c:/downloads/b.bin",
            )

    def test_active_output_path_is_unique_across_different_urls(self):
        output = "c:/downloads/shared.bin"
        self.add("a", "https://one.test/file", output)
        with self.assertRaises(sqlite3.IntegrityError):
            self.add("b", "https://two.test/file", output)

    def test_completed_or_failed_record_releases_active_ownership(self):
        self.add(
            "complete",
            "https://example.test/file",
            "c:/downloads/shared.bin",
            status=3,
        )
        self.add(
            "failed",
            "https://other.test/file",
            "c:/downloads/other.bin",
            status=4,
        )

        self.add(
            "active-url-reuse",
            "https://example.test/file",
            "c:/downloads/new.bin",
        )
        self.add(
            "active-output-reuse",
            "https://new.test/file",
            "c:/downloads/shared.bin",
        )

    def test_late_output_claim_collision_is_atomic(self):
        self.add("a", "https://one.test/file", "c:/downloads/a.bin")
        self.add("b", "https://two.test/file", "")

        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(
                """
                UPDATE http_downloads
                SET save_path = ?, file_name = ?, output_key = ?
                WHERE record_id = ?
                """,
                (
                    "C:/Downloads",
                    "a.bin",
                    "c:/downloads/a.bin",
                    "b",
                ),
            )

        self.assertEqual(
            self.db.execute(
                "SELECT output_key FROM http_downloads WHERE record_id = 'b'"
            ).fetchone(),
            ("",),
        )

    def test_unknown_outputs_can_coexist_until_they_are_claimed(self):
        self.add("a", "https://one.test/file", "")
        self.add("b", "https://two.test/file", "")

        self.db.execute(
            "UPDATE http_downloads SET output_key = ? WHERE record_id = 'a'",
            ("c:/downloads/resolved.bin",),
        )
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(
                "UPDATE http_downloads SET output_key = ? WHERE record_id = 'b'",
                ("c:/downloads/resolved.bin",),
            )

    def test_legacy_duplicate_cleanup_is_deterministic(self):
        legacy = sqlite3.connect(":memory:")
        self.addCleanup(legacy.close)
        legacy.executescript(SCHEMA_SQL)

        rows = [
            (
                "url-old",
                "https://same.test/file",
                1,
                10,
                "c:/downloads/url-old.bin",
            ),
            (
                "url-preferred",
                "https://same.test/file",
                2,
                20,
                "c:/downloads/url-new.bin",
            ),
            (
                "output-preferred",
                "https://one.test/output",
                5,
                30,
                "c:/downloads/shared.bin",
            ),
            (
                "output-duplicate",
                "https://two.test/output",
                5,
                30,
                "c:/downloads/shared.bin",
            ),
        ]
        for record_id, url, added, completed, output_key in rows:
            legacy.execute(
                """
                INSERT INTO http_downloads (
                    record_id,
                    url,
                    save_path,
                    file_name,
                    name,
                    added_timestamp,
                    completed_size,
                    status,
                    output_key
                )
                VALUES (?, ?, 'C:/Downloads', 'file.bin', 'file.bin', ?, ?, 0, ?)
                """,
                (record_id, url, added, completed, output_key),
            )

        legacy.executescript(ACTIVE_URL_SQL)
        legacy.executescript(ACTIVE_OUTPUT_SQL)

        remaining = {
            row[0]
            for row in legacy.execute(
                "SELECT record_id FROM http_downloads"
            )
        }
        self.assertNotIn("url-old", remaining)
        self.assertIn("url-preferred", remaining)

        # Equal progress/timestamp falls through to record_id lexical order.
        self.assertIn("output-preferred", remaining)
        self.assertNotIn("output-duplicate", remaining)

        self.assertEqual(
            legacy.execute(
                """
                SELECT COUNT(*)
                FROM http_downloads
                WHERE status < 3
                """
            ).fetchone()[0],
            2,
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
