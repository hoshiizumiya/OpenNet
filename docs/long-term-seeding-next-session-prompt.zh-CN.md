# 下一轮对话接手提示词：OpenNet Long-Term Seeding / HTTP P2P


> **架构更新 — 2026-09-24**
>
> HTTP P2P 的首选路径已经改变。对于隐私安全的 public HTTP 资源，若 caller 提供 WholeFile SHA-256，且 size、BEP52 identity、输出路径、HTTP byte-range 能力都满足可信条件，则 OpenNet 让 aria2 保持 paused，只临时承担现有 GID/UI/SQLite control shell；真正的数据面交给 canonical BitTorrent v2 session。libtorrent 同时接收 final HTTP origin 作为 BEP 19 URL Seed，并接收 OpenNet peers，因此 HTTP origin + P2P 共用同一个 piece picker、同一套校验路径和单一 disk writer。若没有可信 candidate，或 hybrid 启动/运行失败，则恢复 aria2 普通 HTTP 下载。旧的独立临时文件 full-file fallback 只作为兼容路径保留。
>
> 因此自研 aria2/libtorrent `TransferCoordinator` 不再是 public HTTP 加速的默认设计。核心 invariant 仍然是：`one output range -> one writer/owner`。

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

当前 primary hybrid 由 libtorrent 直接写最终目标，因为 aria2 paused。hybrid 失败必须先关闭 libtorrent、删除不完整目标，再 Resume aria2；绝不能两个 writer 共存。

旧 `.opennet-p2p-<gid>.part` 整文件 fallback 仍作为兼容/恢复路径。

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

### 1. deterministic WebSeed 端到端测试

本地 HTTP Range server 覆盖：

```text
URL -> ResourceKey -> candidate -> wakeup -> manifest
 -> URL Seed + OpenNet peer
 -> BEP52 -> WholeFile SHA-256
 -> HTTP Complete -> ContentCatalog
```

同时记录 libtorrent 对当前 `OpenNet.Content.v1` layout 的真实 URL 请求路径，不要猜。

### 2. lifecycle hardening

- shutdown 可取消 wakeup/lookup；
- Pause/Resume 后重新 discovery；
- abnormal exit 后 stale partial cleanup；
- same-target duplicate task 排他；
- redirect / Content-Disposition 晚到 filename 后启用 hybrid；
- aria2 control-shell cleanup/session persistence 的失败恢复。

### 3. Traversal / security / production

- verified IPv4/IPv6 candidates；
- NAT hole punching / relay；
- node key / signed requests / Peer Ticket；
- rate limiting / migrations。

### 4. BitComet

继续可复现黑盒实验，但不阻塞 native OpenNet protocol。

`TransferCoordinator` 不再是普通 HTTP P2SP 的下一步；只有未来 source 无法表示为 libtorrent URL Seed / Peer 时再设计。

## 工作方式

- 先读实际源码再改。
- 直接推进代码，不只给方案。
- 不等待长 Canary；只处理真实 compiler/test failure。
- 不读巨大的 CI log；优先使用用户给出的错误片段或精确 job 输出。
- 一轮集中一个 coherent slice，最后一次性 fast-forward feature branch。
- 不把未验证行为写成事实。
