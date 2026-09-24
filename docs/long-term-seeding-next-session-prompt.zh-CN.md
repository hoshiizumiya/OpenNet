# 下一轮对话接手提示词：OpenNet Long-Term Seeding / HTTP P2P

把下面整段直接交给下一轮 ChatGPT/Codex 使用。

---

你正在继续 OpenNet 的“长效种子 / HTTP P2P 加速”实现。不要重新从概念讨论开始，先读取实际仓库、feature branch 和 checkpoint 文档，再继续编码。

相关仓库：

- OpenNet client: https://github.com/hoshiizumiya/OpenNet
- OpenNet.Server: https://github.com/Millennium-Science-Technology-R-D-Inst/OpenNet.Server
- OpenNet.Traversal: https://github.com/Millennium-Science-Technology-R-D-Inst/OpenNet.Traversal

当前 feature branch：

- OpenNet: `feat/long-term-seeding-content-catalog`
- OpenNet.Server: `feat/long-term-seeding-directory`

首先完整阅读：

- `docs/long-term-seeding-architecture.md`
- `docs/long-term-seeding-architecture.zh-CN.md`
- `docs/long-term-seeding-development-checkpoint.md`

核心 invariant：

```text
Task lifetime != Content lifetime != Seed-session lifetime
```

不要破坏它。

## 已实现，不要重复造轮子

### Content / identity

- 独立 SQLite `content_catalog.db`
- multiple identities / locations / sources
- `Bep52FileRootSha256 = 1`
- `WholeFileSha1 = 2`
- `WholeFileSha256 = 3`
- `BitCometLtSeed160Opaque = 128`
- HTTP/v1 后台 BEP52-compatible + WholeFile SHA-256 hashing
- v2/hybrid 直接复用 libtorrent per-file root
- canonical 1 MiB piece-root cache
- private torrent 排除

不要把 generic SHA-1 再叫 `Bep47WholeFileSha1`。

### Content Directory / canonical swarm

- node inventory / generation / persisted registration idempotency
- lease heartbeat
- content alias merge / conflict rejection
- bounded lookup
- demand-driven wakeup
- ready TTL
- deterministic `OpenNet.Content.v1/content` BitTorrent v2 swarm
- Server canonical manifest validation
- requester manifest validation
- hidden seed/download sessions
- TCP/uTP peer injection

### HTTP ResourceKey discovery

已经实现两个 ResourceKey：

```text
1 = ExactUrlSha256V1
2 = HttpValidatorSha256V1
```

`ResourceKey != ContentIdentity`。

Exact key 使用规范化 public HTTP(S) URL（exact query 参与 hash）；validator key 使用 final URL + strong ETag + Content-Length。

只向 Server 发送 SHA-256 digest，不上传 raw URL。

以下情况当前直接禁止共享 ResourceKey：

- explicit Cookie
- Username / Password
- custom Headers
- Referer
- User-Agent override
- URL credentials
- query name 明显包含 token/signature/credential/session 等认证信息
- 常见 cloud signed-query 前缀

strong ETag 不是 cryptographic content hash。

Server 已实现：

```http
POST /api/v1/content/nodes/{nodeId}/resources
GET  /api/v1/content/resources/lookup
```

ResourceObservation 有 TTL，并且必须绑定当前 node 的 ContentPresence；inventory 删除 Content 时 stale mapping 会失效。

### HTTP verified full-file fallback

HTTP creation chain 已确认：

```text
HttpDownloadDialog::StartDownloadAsync
  -> DownloadManager::AddHttpDownload(options)
  -> Aria2Instance::AddUriWithOptions
  -> aria2.addUri
```

preflight：

```text
HEAD
 -> 必要时 GET Range: bytes=0-0
 -> final redirected URL
 -> Content-Length
 -> strong ETag
```

Resource lookup 在独立 worker 中运行，不阻塞 `aria2.addUri`。

自动 P2P fallback 只有在以下条件同时满足时才允许：

1. caller 明确提供 whole-file SHA-256；
2. Resource lookup candidate 中存在完全相同的 `WholeFileSha256` alias；
3. preflight size 已知时必须相等；
4. candidate 有 BEP52 identity；
5. 启动任务时已经知道 output filename。

数据路径严格分离：

```text
aria2:      target.file
libtorrent: target.file.opennet-p2p-<gid>.part
```

绝对没有两个 engine 同时写同一个文件。

peer fallback worker：

- 最多 2 个并发；
- 使用现有 hidden canonical v2 download；
- polling `LongSeedSessionStatus`；
- libtorrent 完成后再用 ContentHasher 重算 whole-file SHA-256；
- size + caller SHA-256 都匹配才进入 Ready；
- origin 先完成 -> cancel/remove peer temp；
- origin Error + peer Ready -> `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)` 原子 promotion；
- promotion 后 HTTP record 改 Complete，最终文件进入 ContentCatalog；
- Pause/Cancel/Remove/Delete 会取消并 suppress late lookup，防止用户停止后隐藏 P2P 又被迟到结果重新启动。

这仍然是 **full-file fallback / alternative source**，不是 mixed-source P2SP。

## 绝对不要做的事

- 不要让 aria2 和 libtorrent 无协调地写同一路径。
- 不要把 URL / ETag 当 Content Hash。
- 不要把 signed/private raw URL、cookie、token 上传 Server。
- 不要因为 Server resource mapping 命中就信任 payload。
- 不要现在为 SSL/Noise/PQC 重写数据面。
- 不要假设 BitComet `filehash == SHA1(file)`。
- 不要假设 BitComet LT UDP == uTP。
- 不要等待一个小时以上的 Windows Canary 才继续开发。

## 下一轮优先任务

### 1. 先做真实端到端 fallback 测试

覆盖完整链：

```text
HTTP URL
 -> ResourceKey
 -> Server resource candidate
 -> content lookup prepare
 -> seeder wakeup
 -> canonical manifest
 -> hidden libtorrent peer download
 -> BEP52 verify
 -> local WholeFile SHA-256 verify
 -> origin Error
 -> atomic promotion
 -> HTTP Complete
 -> ContentCatalog
```

优先寻找能够在仓库测试体系中稳定模拟的边界，不要依赖公网不确定 Peer。

### 2. 收紧 fallback 生命周期

重点检查：

- shutdown 时正在进行的 12s wakeup/lookup 是否可取消；
- Resume 后是否需要重新 discovery；
- output filename 由 server/redirect/Content-Disposition 晚到时如何安全开启 fallback；
- aria2 Error 后 output handle 延迟释放时 promotion 重试；
- app crash 后遗留 `.opennet-p2p-*.part` 如何清理；
- promotion 成功但 aria2 remove/save-session 失败时的恢复语义；
- duplicate HTTP task / same target path 的排他性。

### 3. 才开始 TransferCoordinator

真正 origin + P2P 同时加速前，必须建立：

```text
one output range -> one current writer/owner
```

需要先设计并测试：

- range ownership
- verification boundary
- retry/reassignment
- sparse/preallocation
- resume
- truncation
- engine cancellation
- final commit/rename

不要直接“让 aria2 + libtorrent 都指向同一个文件”。

### 4. 后续

- OpenNet.Traversal verified IPv4/IPv6 candidate
- NAT hole punching
- node key / signed control requests
- Peer Ticket
- rate limiting / production migrations
- BitComet 黑盒兼容实验

## 工作方式

- 先查实际源码再改。
- 直接推进代码，不要只给方案。
- 不要问仓库里已经能得出的澄清问题。
- 一轮集中一个大开发切片。
- detached/draft commits 可以连续做，最后只更新 feature branch 一次，避免 Canary 反复触发。
- 提交前做跨文件、跨仓静态审查。
- CI 真正给出 compiler/test failure 后再集中修。
- 不要把未验证行为写成事实。
