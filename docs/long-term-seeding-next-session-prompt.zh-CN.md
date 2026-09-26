# 下一轮对话接手提示词：OpenNet Long-Term Seeding / HTTP P2P


> **架构更新 — 2026-09-26**
>
> HTTP Task 已分成 `Aria2Only` 与 `P2PPreferred`。只有 `P2PPreferred` 会先创建 paused aria2 control shell，再向 OpenNet Content Directory 获取并验证 canonical manifest/info-hash；源 HTTP Server 返回普通 SHA-256 不能直接生成 magnet。canonical metadata / caller SHA-256 / size / Range / privacy 任一条件不足，或 libtorrent hybrid 失败，就回落 aria2。
>
> Pause/Resume 已改为真正 pause/resume hidden libtorrent session，不再把 Pause 当 Cancel。canonical `OpenNet.Content.v1/content` 保持不变；direct-file URL Seed 与 trailing-slash base URL 的请求路径已加入 deterministic local Range-server tests。

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

### HTTP canonical WebSeed hybrid

HTTP 下载模式已显式分流：

```text
Aria2Only
  -> aria2 直接下载

P2PPreferred
  -> canonical discovery / trust checks
  -> success: libtorrent URL Seed + peers
  -> unavailable/failure: aria2 fallback
```

不要把 HTTP Server 返回的 WholeFile SHA-256 当作 magnet。这个 SHA-256 只用于确认候选内容与 caller 期望相同；真正的 BEP52 identity、canonical manifest 和 v2 info-hash 必须从已经登记的 Content/Directory 数据取得.

可信 public HTTP 的主路线已经改成：

```text
active Range probe: bytes=0-0 -> 206 + Content-Range
        |
        v
ResourceKey -> authoritative candidate
        |
        v
paused aria2 control shell（不写 payload）
        |
        v
canonical v2 libtorrent session
  + BEP19 HTTP URL Seed
  + OpenNet TCP/uTP peers
        |
        v
one piece picker / one writer
```

启用条件必须同时包含 caller WholeFile SHA-256、candidate exact SHA-256 alias、size、BEP52 identity、已知 output path、privacy-safe public request，以及真实 206 + Content-Range。`Accept-Ranges` 只能用于 UI 提示。

`StartLongSeedDownloadAsync` -> `OpenLongSeedDownloadSession` -> `add_torrent_params::url_seeds` 已经贯通。URL Seed 存在时，不要求启动前已经有 ready Peer。

当前 primary hybrid 由 libtorrent 直接写最终目标，因为 aria2 paused。hybrid 失败必须先关闭 libtorrent、删除不完整目标，再在用户没有主动 Pause 时 Resume aria2；绝不能两个 writer 共存。

HTTP dialog 已有 `P2P acceleration` 开关，默认开启：开启映射到 `P2PPreferred`，关闭映射到 `Aria2Only`.

Pause active hybrid 会调用 hidden libtorrent session pause；Resume 恢复同一 session。Cancel/Remove/Delete 才关闭 session 并 suppress late discovery。

HTTP record 现在持久化：
- `transferMode`；
- `activeEngine`；
- `userRequestedPaused`；
- `canonicalInfoHashV2`。

caller WholeFile SHA-256 也作为本地 recovery authority 保存。应用重启时，如果上次 hidden canonical session 已消失：
- Probe 阶段可以直接安全回落 aria2；
- Hybrid 阶段先重新 hash 最终文件；
- SHA-256/size 完整匹配则恢复 HTTP Complete；
- 否则删除 libtorrent partial，确认目标释放后才 Resume aria2。

TasksPage 名称下方现在直接显示 `P2P discovery` / `libtorrent · HTTP/P2P` / `aria2`，hybrid 的 download/upload rate、peer/seed 连接数也投影进现有 HTTP task telemetry。Task Log 会记录实际 canonical v2 info-hash。

依赖基线已升级并显式约束：
- libtorrent >= 2.1.2；
- sentry-native >= 0.17.1。
OpenNet 没有使用 libtorrent 2.1.2 新弃用的内部 file_storage helper，也没有使用 sentry-native 0.17.x 发生 breaking change 的 attachment/before_send/on_crash/logs/metrics API，因此不要为了版本升级制造无关迁移。

WebSeed path fixture 已加入：
- 完整 direct-file URL `/file.bin` 必须原样请求；
- trailing-slash `/origin/` 会被 libtorrent 视为 base URL，并追加 `OpenNet.Content.v1/content`；
- 新增 v2 multi-file 非 piece 对齐边界 fixture，覆盖 libtorrent 2.1.2 的 WebSeed request-coalescing regression 类别。

因此自动 WebSeed 只接受不以 `/` 结尾的 direct-file final URL。canonical protocol/layout 此轮没有修改，Server validator 不需要同步变化.

旧 `.opennet-p2p-<gid>.part` separate-file fallback 已从 runtime 删除；当前 HTTP/P2P 只有 canonical libtorrent 一个数据 writer。启动时仅按持久化 target + GID 精确删除旧 prototype 遗留 temp，不做目录通配清理。

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

### 1. 处理真实编译/测试反馈

当前 client code checkpoint：

- `4d130bf779f703754bb692ae2fefd3d62479ee44`

不要等待 Canary。若出现具体 compiler/test error，只针对错误集中修一批.

### 2. 完整 deterministic hybrid integration test

在现有 local HTTP Range fixture 之上继续覆盖：

```text
ResourceKey
 -> Directory candidate
 -> canonical manifest
 -> URL Seed + local OpenNet peer
 -> BEP52
 -> caller WholeFile SHA-256
 -> HTTP Complete
 -> ContentCatalog
```

### 3. lifecycle / persistence hardening

当前 restart recovery 已有原型实现，下一步是补 deterministic crash/restart tests，而不是重新设计：
- Content Directory shutdown cooperative cancellation 已落地：请求有 deadline，DownloadManager / sync service 会通过 stop token 取消正在进行的 HTTP operation；
- restart 时 complete-vs-partial SHA recovery 测试；完整 canonical payload 的 recovery 已补齐 ContentCatalog replay，避免 HTTP Complete 与长期做种 catalog 在 crash 后分叉；
- aria2 shell removal failure / locked partial cleanup 的 blocked recovery 测试；
- normalized output_key / restored-GID ownership crash 测试；
- Resume re-discovery 已落地保守版本：只在 aria2 Paused + CompletedLength=0 + payload 可释放 + 已持久化可信 WebSeed/SHA/size/path 时重新进入 CanonicalProbe；
- redirect / Content-Disposition 晚到 filename 因此也能在上述零进度 Resume 边界重新评估；已有 aria2 partial 不热切换，避免丢进度或双 writer。

### 4. Traversal / security / production

- verified IPv4/IPv6 candidates；
- NAT hole punching / relay；
- node key / signed requests / Peer Ticket；
- rate limiting / migrations.

### 5. BitComet

继续可复现黑盒实验，但不阻塞 native OpenNet protocol.

`TransferCoordinator` 不再是普通 HTTP P2SP 的默认方案；只有 source 无法表示为 libtorrent URL Seed / Peer 时再设计.

## 工作方式

- 先读实际源码再改。
- 直接推进代码，不只给方案。
- 不等待长 Canary；只处理真实 compiler/test failure。
- 不读巨大的 CI log；优先使用用户给出的错误片段或精确 job 输出。
- 一轮集中一个 coherent slice，最后一次性 fast-forward feature branch。
- 不把未验证行为写成事实。
