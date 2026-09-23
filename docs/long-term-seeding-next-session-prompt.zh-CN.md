# 下一轮对话接手提示词：OpenNet Long-Term Seeding / HTTP P2P

把下面整段直接交给下一轮 ChatGPT/Codex 使用。

---

你正在继续 OpenNet 的“长效种子 / HTTP P2P 加速”实现。不要重新从概念讨论开始，先读取实际仓库和现有 feature branch，再继续编码。

相关仓库：

- OpenNet client: https://github.com/hoshiizumiya/OpenNet
- OpenNet.Server: https://github.com/Millennium-Science-Technology-R-D-Inst/OpenNet.Server
- OpenNet.Traversal: https://github.com/Millennium-Science-Technology-R-D-Inst/OpenNet.Traversal

当前分支：

- OpenNet: `feat/long-term-seeding-content-catalog`
- OpenNet.Server: `feat/long-term-seeding-directory`

关键实现 checkpoint：

- OpenNet implementation commit: `6eecbc78a89b9c05af7f616f5eeb814e12ebf45f`
- OpenNet.Server current implementation commit: `6b344c710092b1c7e41cb9f698abfb02f8d30a48`

注意：OpenNet 分支在 implementation commit 之后还有交接文档提交，因此开始工作时必须先读取分支最新 HEAD，而不是强行 reset 回上面的 checkpoint。

Server 当前 `6b344c710...` 的 CI 已经通过 Restore / Build / Test。

OpenNet 的 Windows Canary 非常慢，通常耗时一个小时以上。**永远不要为了等待 Canary 而停止开发。** 把 CI 当成异步反馈：先批量完成一个开发切片、静态审查、提交，继续开发；真正出现编译错误后再集中处理。不要一行错误推一次 commit。

首先完整阅读：

- `docs/long-term-seeding-architecture.md`
- `docs/long-term-seeding-architecture.zh-CN.md`
- `docs/long-term-seeding-development-checkpoint.md`

当前核心 invariant：

```text
Task lifetime != Content lifetime != Seed-session lifetime
```

不要破坏它。

当前已经实现，不要重复造轮子：

1. Client ContentCatalog：
   - protocol-neutral ContentIdentity
   - multiple identities / locations
   - dedicated SQLite `content_catalog.db`
   - startup metadata validation
   - HTTP completion ingestion
   - torrent completion ingestion
   - private torrent exclusion
   - BEP52-compatible hashing
   - WholeFile SHA-256 alias

2. Identity IDs：
   - 1 = `Bep52FileRootSha256`
   - 2 = `WholeFileSha1`
   - 3 = `WholeFileSha256`
   - 128 = `BitCometLtSeed160Opaque`

   不要重新叫 `Bep47WholeFileSha1`。

3. Canonical swarm：
   - deterministic single-file BitTorrent v2
   - `OpenNet.Content.v1/content`
   - fixed 1 MiB piece length
   - canonical piece-root cache
   - v2/hybrid 可在第一次 wakeup 时 lazy 补 canonical piece layer
   - Server 会验证 canonical bencode / raw info hash / BEP52 root / piece layer
   - requester 在交给 libtorrent 写文件之前还会再验证一次 manifest

4. Content Directory：
   - node inventory register
   - random NodeId
   - generation
   - persisted registration idempotency
   - lease heartbeat
   - bounded lookup
   - observed source address + client-submitted port
   - expired cleanup
   - wakeup queue
   - wakeup completion
   - ready TTL
   - canonical manifest endpoint
   - inventory refresh 不会把 unchanged content 的 ready state 清掉

5. Hidden libtorrent data plane：
   - on-demand hidden seed session
   - seed_mode
   - canonical local-path mapping
   - hidden download session
   - requester excludes own NodeId
   - TCP/uTP peer injection
   - DHT/LSD/PEX disabled for directory-driven hidden session
   - temporary hidden sessions do not become normal user tasks

重要限制：

- **绝对不要让 aria2 和 libtorrent 无协调地同时写同一个目标文件。**
- 当前 P2P requester 是 standalone full-file transport primitive。
- 真正 HTTP P2SP 混源之前必须有 `TransferCoordinator` / range ownership。
- 当前 endpoint 还是 `observed-address-unverified-port`，没有通过 Traversal 证明可达性。
- 不要把 ETag 当 cryptographic hash。
- 不要把 signed/private URL、cookie、token、authorization query 直接上传 Server。
- 不要现在为了 SSL/Noise/PQC 重写 peer transport；native v1 先复用 libtorrent。
- BitComet LT hash / wire protocol 仍是 proprietary unknown；不要假设 SHA1(file)，不要假设 LT UDP == uTP。

## 你这一轮的主要目标

继续推进 **HTTP ResourceKey -> Content discovery**，让一个新建 HTTP/HTTPS 任务在完整文件还没有下载完成前，有机会发现 Server 已知的同内容 canonical swarm。

不要直接开始混合写入。先完成 discovery 与安全模型。

### 第一步：必须先查实际源码

先定位并解释：

1. 新 HTTP task 从 UI/VM 到 `DownloadManager` 再到 aria2 的真实创建调用链。
2. task 创建之前有哪些信息：
   - URL
   - headers
   - output path
   - expected size（如果有）
   - ETag（什么时候才能有）
   - Last-Modified
   - redirect 后 final URL
3. 当前 aria2 wrapper 是否有 HEAD / response header / final effective URI 等能力。
4. 哪一层最适合做 resource discovery，不要凭感觉选。

给出精确文件、类、函数，并区分 verified source fact 与设计建议。

### 第二步：设计 ResourceDescriptor / ResourceKey

目标：

```text
origin resource observation
      |
      v
privacy-preserving ResourceKey
      |
      v
Server resource hint
      |
      v
ContentId + authoritative identity
```

设计原则：

- ResourceKey != ContentId
- URL/ETag 只是 discovery hint，不是完整性证明
- query 参数默认敏感/有意义，不能随便丢
- signed URL 必须避免把 token 泄漏到 Server
- 可以考虑对 canonicalized descriptor 做本地 hash 后只上传 digest
- strong ETag + Content-Length 能增强映射可信度
- redirect/final URL 语义要明确
- mapping 要有 observed time / expiry / validator metadata
- 同一个 ResourceKey 发生 content 变化时要避免污染旧映射

先写协议/domain model，再写 storage/API。

### 第三步：Server 落地

在 OpenNet.Server feature branch 上实现：

- resource hint entity / indexes
- resource announce
- resource lookup
- conflict/update policy
- expiration / retention 基础
- bounded API
- 不保存 raw secret-bearing URL
- tests

尽量复用现有 ContentObject，不另建一套 Content identity。

### 第四步：Client 落地

实现：

- ResourceDescriptor / ResourceKey
- 在正确的 HTTP task 生命周期位置执行 lookup
- lookup 命中后拿到 authoritative BEP52/ContentId 时，可调用现有 hidden P2P download primitive
- 第一版允许策略是“选择 P2P full-file 或 origin”，或者 P2P 失败再 origin fallback
- **不要**让 aria2 + libtorrent 同时写同一路径

如果现有 UI/task model 还不适合直接切换 source，先把 discovery service/API 做完整并提供明确 hook，不要为了赶功能破坏任务状态机。

### 第五步：测试与文档

- Server unit/integration tests
- ResourceKey test vectors
- signed URL / query privacy cases
- changed ETag / changed Content-Length cases
- redirect cases
- update architecture 中英文文档
- 更新 development checkpoint

## 后续，但不是本轮优先

在 ResourceKey discovery 稳定以后再做：

1. requester -> lookup -> seeder wakeup -> manifest -> TCP/uTP payload 的端到端测试
2. `TransferCoordinator` / range ownership
3. HTTP origin + P2P 并行 scheduler
4. OpenNet.Traversal verified IPv4/IPv6 candidates
5. hole punching
6. node key/signature
7. Peer Ticket
8. rate limiting / production migrations
9. BitComet 黑盒兼容研究

## 工作方式要求

- 直接推进代码，不要只给方案。
- 搜索并读取真实仓库后再改。
- 不要问已经能从仓库/文档得出的澄清问题。
- 相关改动按一个开发切片集中提交，避免频繁 push 触发 Canary。
- OpenNet Canary 很慢：不要等待超过几分钟，更不要等待一个小时；继续推进下一批可以独立完成的工作。
- 每次提交前做跨文件静态审查。
- 如果 CI 之后失败，再读取实际日志集中修复。
- 不要把未验证的 BitComet / libtorrent 行为当成事实。

开始时先汇报你从源码确认到的 HTTP task creation call chain，然后直接进入 ResourceKey 第一阶段实现。
