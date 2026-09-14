# libtorrent 2.1.1 audit implementation status

Updated: 2026-09-07. This is a work-in-progress implementation record for the
2026-09-04 audit, not a declaration that all 13 delivery stages passed acceptance.

## Implemented paths

- Structured core add results and v1/v2 identity lookup; duplicate additions do
  not publish a new task mapping. Task metadata and hash aliases share a SQLite
  transaction. Import preserves existing local paths/settings and resume data.
  New handles stay paused until persistence and task mapping have completed.
- Pure v2 piece views avoid the SHA-1-only hash accessor. File snapshots expose
  original/current paths and pad flags; native and web file lists hide pads while
  retaining engine indices.
- Shared preview/main network policy adapter, correct RC4 preference, preview
  default-dont-download flag, IP filter inheritance and reusable metadata cache.
- RSS new-item callbacks/download dispatch occur outside the subscription lock.
- Resume checkpoints, retry marking on database save failure, strict public
  torrent export and task-ID cache filenames. Restore falls back from resume data
  to the task cache or magnet; restored storage settings remain task-specific.
- Rename/move/delete alerts are handled. Content-deletion failures retain task
  metadata. Completion-triggered moves resume according to the captured action.
- Share limits are persisted and evaluated in the alert loop. An explicit pause
  is preserved when a share limit stops a task. Completion action is stored per
  task; old completed tasks without it remain stopped. The native settings form
  preserves policy fields it does not display.
- Stable-format share requests can omit the newer mode field. Unsupported path
  management, search execution and web RSS rules return an explicit unsupported
  response. TLS buffers can be applied at runtime; private keys are not returned
  by the TLS read endpoint.
- Peers, file progress, file priorities, tracker lists and piece availability use
  alert-driven caches. Priority and tracker cold reads obtain engine values where
  mutation consumers need confirmed state. Late detail alerts for removed tasks
  are discarded. A read-heavy task-policy cache uses C++23 std::flat_map; no
  measured performance gain is claimed.

## Validation

`python scripts/test_torrent_persistence_sql.py` exercises production SQL with
in-memory SQLite: metadata/resume preservation, policy/completion round trips,
alias conflict rollback, targeted cascading deletion, and legacy column migration.
It does not test C++ locks, WinUI behavior, or live libtorrent sessions.

Build the project with the solution directory explicitly supplied, to use the
same intermediate/output paths as Visual Studio:

```powershell
msbuild.exe OpenNet\OpenNet.vcxproj /t:Build /m:1 /p:Configuration=Debug /p:Platform=x64 /p:SolutionDir=C:\Files\OpenNet\ /v:minimal
```

## Remaining acceptance and implementation

- Full requested-field patches with serialized read/modify/write, explicit run
  intent/stop reason, and durable file-operation records including crash replay.
- End-to-end duplicate/concurrent add and rollback tests, hybrid alias conflict
  review, and recovery under process termination and disk/SQLite failures.
- Complete v2 root/piece-layer display and cache-completeness metadata.
- Share-policy edge cases: selected-file completion duration, persistent idle
  upload duration, zero-download ratio behavior, action inheritance and migration
  of legacy web-only overrides. Runtime scenarios T07/T08/T25 are not yet passed.
- TLS certificate parsing/validation, secure persistence and restart replay.
  Current TLS application is runtime-only.
- Snapshot age/version reporting, request coalescing, full summary/piece-state
  async reads, and task-count/load performance validation.
- Global numeric input rejection before every UI narrowing conversion; current
  normalization and task-rate validation do not cover every import/UI field.
- Unified native/web RSS rules and history, category/temporary-path management,
  bandwidth scheduling, watched directories, and optional real search service.
- Debug/Release and any shipping ARM64 validation, plus live protocol and native/
  web UI tests in the audit's T01–T28 matrix.

No runtime acceptance result is inferred from an HTTP success response or a
successful compilation.
