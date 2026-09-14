"""Exercise production torrent SQL in an isolated in-memory SQLite database.

This checks schema/query compatibility and rollback semantics, not C++ threading
or libtorrent integration. Run: python scripts/test_torrent_persistence_sql.py
"""
import pathlib
import re
import sqlite3
import unittest

SOURCE = (pathlib.Path(__file__).resolve().parents[1] / "OpenNet/Core/torrentCore/TorrentStateManager.cpp").read_text(encoding="utf-8-sig")


def raw_sql(section):
    return re.search(r'R"\((.*?)\)"', section, re.S).group(1)


def method_sql(name):
    return raw_sql(SOURCE.split("TorrentStateManager::" + name + "(", 1)[1])


class PersistenceTests(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(":memory:")
        self.addCleanup(self.db.close)
        self.db.execute("PRAGMA foreign_keys = ON")
        for table in ("createTasksTable", "createTaskSettingsTable", "createHashAliasesTable", "createMigrationConflictsTable", "createSessionTable"):
            self.db.executescript(raw_sql(SOURCE.split("const char* " + table + " =", 1)[1]))

    def add_task(self, task_id="a", hash_v1="abc", queue=7, blob=b"resume"):
        values = (task_id, "magnet:?xt=urn:btih:" + hash_v1, "C:/downloads", "test", 1,
                  100, 50, 20, 0, 2, 1, hash_v1, "", "", queue, blob)
        self.db.execute(method_sql("SaveTaskMetadata"), values)

    def test_metadata_columns_and_resume_preservation(self):
        self.add_task()
        self.add_task(queue=-1, blob=None)
        row = self.db.execute(method_sql("LoadTaskMetadata"), ("a",)).fetchone()
        self.assertEqual(row[0], "a")
        self.assertEqual(row[14], 7)
        self.assertEqual(row[15], b"resume")
        self.assertEqual(len(self.db.execute(method_sql("LoadAllTasks")).fetchall()), 1)

    def test_completion_and_share_policy_roundtrip(self):
        self.add_task()
        values = ("a", 1024, 2048, 0, -1, -1, 1, 0, 1, 1, 0, 0, 0, 0, 0,
                  1.75, 120, -1, 1, 0, 1)
        self.db.execute(method_sql("SaveTaskSettings"), values)
        row = self.db.execute(method_sql("LoadTaskSettings"), ("a",)).fetchone()
        self.assertEqual(row, values[1:])
        self.assertEqual(row[-1], 1)

    def test_alias_conflict_rolls_back_new_task(self):
        self.add_task()
        self.db.execute("INSERT INTO torrent_hash_aliases VALUES ('a', 1, 'abc')")
        self.db.commit()
        with self.assertRaises(sqlite3.IntegrityError):
            with self.db:
                self.add_task("b", "ABC")
                self.db.execute("INSERT INTO torrent_hash_aliases VALUES ('b', 1, 'ABC')")
        self.assertIsNone(self.db.execute("SELECT task_id FROM tasks WHERE task_id='b'").fetchone())
        self.assertEqual(self.db.execute(method_sql("FindTaskIdByInfoHashes"), ("ABC", "")).fetchone(), ("a",))
        self.assertEqual(self.db.execute("SELECT resume_data FROM tasks WHERE task_id='a'").fetchone()[0], b"resume")

    def test_deletion_cascades_only_target_task(self):
        self.add_task()
        self.add_task("b", "def")
        self.db.execute("INSERT INTO torrent_hash_aliases VALUES ('a', 1, 'abc')")
        self.db.execute("INSERT INTO task_settings(task_id) VALUES ('a')")
        self.db.execute("DELETE FROM tasks WHERE task_id='a'")
        self.assertEqual(self.db.execute("SELECT count(*) FROM torrent_hash_aliases").fetchone()[0], 0)
        self.assertEqual(self.db.execute("SELECT count(*) FROM task_settings").fetchone()[0], 0)
        self.assertEqual(self.db.execute("SELECT task_id FROM tasks").fetchall(), [("b",)])

    def test_legacy_schema_accepts_all_column_migrations(self):
        old = sqlite3.connect(":memory:")
        self.addCleanup(old.close)
        old.execute("CREATE TABLE tasks(task_id TEXT PRIMARY KEY, magnet_uri TEXT, save_path TEXT, name TEXT, added_timestamp INTEGER, total_size INTEGER, downloaded_size INTEGER, status INTEGER, resume_data BLOB)")
        old.execute("CREATE TABLE task_settings(task_id TEXT PRIMARY KEY)")
        migrations = re.findall(r'"(ALTER TABLE \w+ ADD COLUMN [^"]+;)"', SOURCE)
        for sql in migrations:
            old.execute(sql)
        self.assertIn("completion_action", [r[1] for r in old.execute("PRAGMA table_info(task_settings)")])
        self.assertIn("queue_position", [r[1] for r in old.execute("PRAGMA table_info(tasks)")])


if __name__ == "__main__":
    unittest.main(verbosity=2)
