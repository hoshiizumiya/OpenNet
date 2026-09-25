# OpenNet 长效种子与 HTTP P2P 加速架构


> **架构更新 — 2026-09-26**
>
> HTTP Task 现在有明确的传输策略。`Aria2Only` 直接启动 aria2，不做下载前 P2P discovery；`P2PPreferred` 才优先尝试可信 canonical 路线：aria2 只创建 paused control shell，OpenNet 通过 Content Directory 获取 canonical manifest/info-hash，libtorrent 再把 final HTTP URL 作为 BEP 19 URL Seed 与 OpenNet peers 一起调度。拿不到可信 canonical metadata 或 hybrid 失败时，才把写入权交回 aria2。
>
> HTTP 源站即使返回普通 WholeFile SHA-256，也不能把它“直接变成 magnet”。WholeFile SHA-256 只负责确认整文件身份；BitTorrent v2 info-hash / BEP52 file root 必须来自已经构建并验证过的 canonical torrent metadata。核心 invariant 仍然是：`one physical output -> one active writer`。

> 状态：架构基线与实现指南。
>
> 本文所述的本地 ContentCatalog、Content Directory 控制面、按需 canonical BitTorrent v2 swarm、HTTP ResourceKey 提前发现，以及“独立临时文件 + SHA-256 二次校验”的整文件 P2P fallback 已经在对应 feature 分支中实现。aria2/libtorrent 真正混合 range 加速、节点密码学认证、Peer Ticket、经过验证的 NAT 穿透仍属于后续阶段。

## 1. 目标

OpenNet 长效种子的目标，是让一个已经完成并经过校验的文件，在原下载任务进入 inactive / stopped 状态之后仍然能够继续产生 P2P 价值。

整个设计最重要的不变量是：

```text
Task 生命周期 != Content 生命周期 != Seed Session 生命周期
```

Task 是用户界面和下载引擎中的任务对象；Content 是“本机仍然保存着某份经过验证的内容”这一持久事实；Seed Session 是只有在别人真正请求该内容时才临时创建的数据传输对象。

这样做可以同时实现：

- HTTP 任务停止后，其完整文件仍可继续提供；
- torrent 任务停止甚至从 libtorrent 活跃会话中卸载后，本地文件仍可被发现；
- 来自不同 torrent、不同 HTTP URL 的完全相同文件可以汇聚到同一个逻辑 Content；
- 不需要为了“长效”让所有历史 torrent 永久常驻 libtorrent；
- 控制面与实际 Peer 数据面可以独立演进；
- 未来兼容 BitComet 时，只需要增加兼容适配器，而不需要更换 OpenNet 原生内容身份体系。

## 2. 三个仓库的职责边界

### OpenNet 客户端

仓库：`hoshiizumiya/OpenNet`

职责：

- 维护持久化的本地 `ContentCatalog`；
- 生成或直接复用文件的密码学身份；
- 判断目录中记录的本地文件位置是否仍然有效；
- 把当前真正可用的 Content inventory 注册到 OpenNet.Server；
- 使用 heartbeat 维持 node lease；
- 根据 ContentIdentity 查询 Peer candidates；
- 按需在 libtorrent 中建立隐藏的 canonical seed/download session；
- 下载前用权威 BEP 52 file root 再验证 Server 返回的 canonical manifest。

### OpenNet.Server

仓库：`Millennium-Science-Technology-R-D-Inst/OpenNet.Server`

职责：

- 把多个 ContentIdentity alias 映射到一个逻辑 Content；
- 记录当前哪些仍处于有效 lease 的 Node 拥有这些 Content；
- 维护 Node endpoint candidate 与 lease 过期状态；
- 根据 ContentIdentity 查询并返回数量受限的 Peer；
- 拒绝冲突 alias 和过期 generation；
- 不能相信客户端随意声明的公网 IP；
- 后续增加 Node 身份认证和短生命周期 Peer Ticket。

### OpenNet.Traversal

仓库：`Millennium-Science-Technology-R-D-Inst/OpenNet.Traversal`

Traversal 继续作为独立子系统存在，不能把它和 Content Directory 混成一个服务。

它未来负责通过 STUN/NAT 穿透、直接 IPv6、UPnP/NAT-PMP/PCP，以及必要时的 relay，对客户端网络 candidate 做发现和可达性验证。

Content Directory 回答的是：

```text
“谁声称拥有 Content X？”
```

Traversal 回答的是：

```text
“Node A 到底能通过哪个 endpoint 被其他人真正连上？”
```

这两个问题必须解耦。

## 3. 总体架构

```text
                  +----------------------+
                  |    OpenNet.Server    |
                  |----------------------|
                  | Content identities   |
                  | Content -> nodes     |
                  | Node leases          |
                  | Endpoint candidates  |
                  +----+------------+----+
                       ^            |
       register/       |            | lookup
       heartbeat       |            v
                       |       请求方客户端
                       |
             +---------+---------+
             |  OpenNet 客户端   |
             |-------------------|
             | ContentCatalog    |
             | HTTP task history |
             | Torrent history   |
             | libtorrent        |
             +---------+---------+
                       |
                 后续：candidate
                    可达性验证
                       |
                       v
              +------------------+
              | OpenNet.Traversal|
              +------------------+

针对已知 BEP 52 identity 已实现的数据面 primitive：

请求方 libtorrent <------ TCP / uTP ------> 做种方 libtorrent
                      canonical v2 swarm
```

普通文件数据不经过 OpenNet.Server 中转。

## 4. ContentIdentity 模型

同一份逻辑 Content 可以同时拥有多个身份。

当前稳定的算法编号为：

| ID | 名称 | 长度 | 含义 |
|---:|---|---:|---|
| 1 | `Bep52FileRootSha256` | 32 字节 | BitTorrent v2 每文件 Merkle Root |
| 2 | `WholeFileSha1` | 20 字节 | 可选的整文件 SHA-1 compatibility alias |
| 3 | `WholeFileSha256` | 32 字节 | 通用整文件 SHA-256 alias |
| 128 | `BitCometLtSeed160Opaque` | 20 字节 | 不解释算法含义的 BitComet 长效种子兼容身份 |

本地 `ContentKey` 是随机内部主键，**不是** Content Hash。

这样设计的原因是：

1. 同一份字节未来可以增加新的身份；
2. BitComet 公开资料不足以证明其 160-bit LT hash 的具体计算方式，因此不应该把它猜成 OpenNet 主身份；
3. 未来新增 BitComet alias 时，不应该引发数据库主键和引用关系整体迁移。

Server 依靠下面的唯一约束做 alias 合并：

```text
(algorithm, digest)
```

如果一次注册提交的多个 alias 在数据库中已经分别指向两个不同 Content，Server 会返回 conflict，而不是强行合并错误的数据。

## 5. BEP 52 文件身份

OpenNet 原生优先使用 BitTorrent v2 的每文件 `pieces root`。

BEP 52 对每个非空文件定义了一棵二叉 Merkle Tree：

```text
文件字节
  |
  +-- 16 KiB block -> SHA-256 ----+
  +-- 16 KiB block -> SHA-256 ----+--> SHA-256(left || right) --> ...
  +-- 最后短 block -> SHA-256 ----+
```

关键细节：

- leaf block 固定为 16 KiB，只有最后一个 block 可以更短；
- 最后这个短 block 按实际长度 Hash，而不是先补零再 Hash；
- 为了把树补成 2 的幂，文件末尾之外不存在的 leaf hash 直接使用 32 字节全零 digest；
- Merkle Tree 各层都使用 SHA-256；
- 空文件没有 `pieces root`；
- 字节完全相同的两个文件一定得到相同 root。

规范：[BEP 52](https://www.bittorrent.org/beps/bep_0052.html)。

### 当前客户端行为

对 BT v2 / hybrid torrent，OpenNet 直接从 libtorrent 的 torrent metadata 读取 per-file root，因此不需要为了建立 ContentIdentity 再把整个文件读一遍。

对纯 v1 torrent 和已完成的 HTTP 文件，后台 `ContentHasher` 会计算 BEP52-compatible root，同时记录 WholeFile SHA-256 alias。

这里刻意采用后台 Hash。当前 HTTP 引擎本质上仍然由 aria2 负责，第一阶段没有为了增量 Hash 强行侵入 aria2 的文件写入链路。

## 6. 一个逻辑 Content 可以有多个本地 Location

逻辑 Content 不能只绑定一个路径。

例如：

```text
Content A
  identities:
    BEP52 = 4f...
    SHA256 = d2...

  locations:
    D:\Downloads\A.iso
    E:\Torrent\images\A.iso

  sources:
    HTTP record 932...
    torrent task 3ad..., file 0
    torrent task 8f1..., file 4
```

这是必须的：删除其中一个副本，不应该让另一个仍然存在的副本也被错误标记成 unavailable。

当前本地领域模型为：

```text
ContentRecord
  ContentKey
  Size
  Identities[]
  Locations[]
  Sources[]
```

每个 Location 保存：

- 本地路径；
- availability；
- 文件大小；
- last-write 时间；
- 以后可利用的 Windows volume/file ID；
- 最近验证时间。

## 7. 本地 SQLite ContentCatalog

客户端使用独立的 `content_catalog.db`，不和 app settings、HTTP task history 等数据库混用。

当前概念 schema：

```text
content
  content_key PK
  file_size
  created_at

content_identity
  content_key FK
  algorithm
  digest
  UNIQUE(algorithm, digest)

content_location
  local_path PK
  content_key FK
  file_size
  last_write_ticks
  volume_serial nullable
  file_id nullable
  availability
  verified_at

content_source
  content_key FK
  source_kind
  source_id
  source_file_index nullable
```

SQLite 启用 WAL、`synchronous=NORMAL`、foreign keys 和 busy timeout。

## 8. 文件进入 ContentCatalog 的规则

### 8.1 HTTP 完成

aria2 状态第一次切换到 `Complete` 时：

1. OpenNet 读取 aria2 实际输出路径；
2. 每个完成文件进入 `ContentCatalogService` 后台队列；
3. 后台 Worker 执行 Hash；
4. 如果已有任意密码学身份能匹配已有 Content，则复用原 Content；
5. 保存 HTTP record 作为 source reference；
6. Catalog 变更后让 Server inventory 进入待同步状态。

如果用户真正删除了下载文件，对应 Location 也会从 Catalog 删除。

仅删除 Task 记录不能影响其他仍存在的物理副本。

### 8.2 BT v2 / hybrid 完成

对于 public torrent：

1. 跳过 pad file；
2. 跳过 priority=0 的未选择文件；
3. 从 libtorrent 读取 BEP 52 per-file root；
4. 使用当前实际映射文件名和当前 storage path；
5. 把 Location 与已知 root 一起入队；
6. 因为已有权威 root，不再为了得到同一身份全盘读取文件。

### 8.3 纯 BT v1 完成

v1 没有标准的独立 per-file Merkle root，因此 OpenNet 会让完成文件进入后台 BEP52-compatible Hash。

### 8.4 Private torrent

Private torrent 当前明确排除在 OpenNet 长效目录之外。

Private tracker / discovery 限制本身就是该 torrent 分发语义的一部分。如果 OpenNet 再把其文件自动广播到一个全局独立目录，就绕过了原有语义。

未来即使考虑显式 opt-in，也必须单独审查，不属于当前架构。

## 9. 启动校验与 I/O 策略

OpenNet 不能每次启动就重新读取用户几 TB 的历史下载。

因此启动先做廉价 metadata validation：

```text
路径存在？
  |
  +-- 否 -> Missing
  |
  +-- 是 -> size 相同？
                |
                +-- 否 -> Modified
                |
                +-- 是 -> mtime 相同？
                              |
                              +-- 否 -> Modified
                              |
                              +-- 是 -> 保留缓存 availability
```

以后可以接入 Windows File ID / USN Journal 增强这一层。

但必须注意：文件系统 metadata 只能用于“缓存是否可能失效”的快速判断，不能代替网络传输时的密码学完整性验证。

## 10. Content Directory 控制面

第一版 Server API 位于：

```text
/api/v1/content
```

### 10.1 完整 Inventory 注册

```http
POST /api/v1/content/nodes/register
```

示例：

```json
{
  "nodeId": "4ac2d9e2-7d31-4ff5-939a-fcd0a459f14c",
  "generation": 18,
  "registrationId": "8928c0e1-214b-48ac-b020-c12821640b63",
  "previousLeaseId": "b40a6136-8e0c-42bd-a743-e437671f82b7",
  "endpoints": [
    { "transport": "Tcp", "addressFamily": "Ipv4", "port": 6881 },
    { "transport": "Utp", "addressFamily": "Ipv4", "port": 6881 },
    { "transport": "Tcp", "addressFamily": "Ipv6", "port": 6881 },
    { "transport": "Utp", "addressFamily": "Ipv6", "port": 6881 }
  ],
  "contents": [
    {
      "size": 4294967296,
      "identities": [
        {
          "algorithm": 1,
          "digest": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        }
      ]
    }
  ]
}
```

客户端**没有**“公网 IP”提交字段。

Server 使用这次 HTTP control connection 实际观察到的 remote address。

一次成功注册会在数据库事务中完整替换该 Node 上一次 inventory，并返回：

```json
{
  "leaseId": "6c8a08b7-9359-4d33-813e-18ddfcb93437",
  "leaseExpiresAtUtc": "2026-09-23T10:30:00Z",
  "acceptedGeneration": 18,
  "contentCount": 1
}
```

### 10.2 Heartbeat

```http
POST /api/v1/content/nodes/{nodeId}/heartbeat
```

```json
{
  "leaseId": "6c8a08b7-9359-4d33-813e-18ddfcb93437"
}
```

当前默认：

- Server lease：300 秒；
- 客户端大约每 90 秒 heartbeat；
- 网络失败按照有界间隔重试。

过期 Node 不会参与 lookup，并由 Server 后台任务从数据库删除。

### 10.3 Lookup

```http
GET /api/v1/content/lookup?algorithm=1&digest={hex}&maxPeers=20&excludeNodeId={self}
```

返回结果只包含 lease 仍有效的 Node，并限制 Peer 数量。

当前 endpoint verification 明确标记：

```text
observed-address-unverified-port
```

原因是：Server 观察到 source IP，并不能证明客户端上报的 TCP/UDP port 真的能从公网访问。

## 11. Generation、Lease、RegistrationId 分别解决什么问题

这三个字段不能混为一谈。

### Generation

`generation` 是一个 Node 的单调 inventory 版本。

低于 Server 已提交 generation 的请求直接拒绝。

### Lease

`leaseId` 表示客户端掌握当前短生命周期注册状态。

如果一个更高 generation 想覆盖仍有效的 lease，就必须带上 previous lease ID。

这只是状态连续性，不等于密码学身份认证。

### RegistrationId

`registrationId` 是“一次 generation 注册”的幂等键。

客户端会在真正发 HTTP 请求**之前**持久化：

```text
pending generation
pending registration id
```

因此即使发生：

```text
Server 已经提交
      |
HTTP Response 丢失
      |
客户端不知道新 LeaseId
      |
客户端重试
```

它仍然发送相同 generation + registrationId，Server 可以识别这是同一次操作的重放，而不是强迫客户端等未知 lease 自然过期。

同 generation 但不同 registrationId 会被拒绝。

## 12. 为什么客户端同步不能只用一个 dirty 布尔值

考虑：

```text
revision 10 开始注册
          |
          +---- 新下载完成 -> revision 11
          |
revision 10 注册成功
```

如果收到成功响应以后简单执行：

```cpp
dirty = false;
```

revision 11 就丢了。

所以当前客户端使用：

```text
localRevision
syncedRevision
```

每次注册捕获自己的 attempt revision。成功以后只表示这个 revision 已同步；如果请求过程中 localRevision 又增加，下一次注册仍然必须执行。

同样，失败后也不会等待一个已经为真的 dirty 条件，从而避免 CPU/网络 tight loop。

## 13. NodeId 与隐私

Content Directory 使用随机生成并持久化的应用级 UUID 作为 NodeId。

它刻意**不复用**现有由用户名/MachineGuid 派生的 DeviceId。

Content Directory 并不需要硬件身份，把它们复用只会无意义增加跨服务可关联性。

但随机 NodeId **不是身份认证**。真正的密码学 Node 身份属于后续安全阶段。

## 14. Endpoint 安全模型

### 客户端当前可以声明什么

当前注册请求只声明：

- TCP / uTP transport；
- IPv4 / IPv6 address family；
- libtorrent 对应 transport/address family 实际成功监听的 port。

这些值来自 libtorrent 的 `listen_succeeded_alert`，该 alert 能区分 TCP 与 uTP socket；同时客户端会尊重用户当前是否启用了 incoming TCP / incoming uTP。

不能直接声明“我的公网 IP 是什么”。

### Server 保存什么

Server 会先判断当前 HTTP control connection 观察到的是 IPv4 还是 IPv6，丢弃客户端提交的另一地址族 endpoint，只把匹配地址族的 port 与 observed source IP 组合成 candidate。

这样至少避免了最直接的：

```text
恶意客户端注册 victim_ip:443
-> 大量 OpenNet 客户端被诱导连接 victim
```

### 当前仍然没有证明什么

Observed IP 不能证明：

- NAT 把该 port 映射到了公网；
- 防火墙允许 TCP inbound；
- UDP 能到达 libtorrent；
- 反向代理后的 RemoteIpAddress 就是真实客户端 IP。

所以当前 endpoint 是 hint，而不是已验证 candidate。

正式公网部署时，只允许显式配置为 trusted 的反向代理/网段提供 forwarded headers。绝不能无条件相信任意 `X-Forwarded-For`，否则“Server observed address”这一安全边界会失效。

## 15. IPv4 / IPv6 与 Traversal

一次 HTTP 控制连接通常只能观察到一种地址族。

不能因为一次请求来自 IPv6，就凭空推断一个 IPv4 endpoint；反过来也一样。

未来正确流程：

```text
OpenNet 客户端
  |
  +-- direct IPv6 candidate
  +-- observed/mapped IPv4 candidate
  +-- STUN UDP candidate
  +-- UPnP/NAT-PMP/PCP mapping
  |
  v
OpenNet.Traversal / reachability probe
  |
  v
verified candidates
  |
  v
绑定到 Content Directory Node lease
```

Server 最终应当允许一个 Node 同时拥有多个独立验证的 candidate，并记录：

- address family；
- transport；
- priority；
- verification method；
- expiry。

Relay 应该最后再考虑，因为一旦 relay 文件 payload，服务器流量成本模型会完全改变。

## 16. 数据面：Canonical BitTorrent v2 Swarm

第一版 canonical 数据面已经落地。它被实现成隐藏 transport primitive，而不是新的用户可见 torrent Task。

只有 ContentIdentity 不足以让两个 libtorrent peer 直接传输。双方还必须加入相同的 BitTorrent swarm，也就是拥有相同 info-hash。

因此 OpenNet 计划为每份 Content 定义一个确定性的单文件 BitTorrent v2 表示。

拟定的 `OpenNet.Content.v1` info dictionary：

```text
meta version = 2
piece length = 1 MiB                 # 协议固定常量
name = "OpenNet.Content.v1"          # 协议固定常量
file tree:
  "content":
    "":
      length = 精确文件长度
      pieces root = BEP52 root       # 空文件省略
```

以下内容禁止进入 canonical info dictionary：

- 时间戳；
- creator；
- 原始 URL；
- 原文件名；
- tracker。

于是：

```text
相同文件字节
  -> 相同 BEP52 root
  -> 相同 canonical info dictionary
  -> 相同 v2 info-hash
  -> 同一个 OpenNet canonical swarm
```

真实磁盘文件名只属于 storage mapping，不能参与 swarm identity。

对于大文件，v2 piece layers 位于 info dictionary 之外。OpenNet 在 HTTP/v1 完成文件 Hash 时同时生成固定 1 MiB canonical piece roots 并持久化到 `content_catalog.db`；v2/hybrid 文件初始只有权威 file root 时，则在第一次真实 wakeup 需求出现时补扫一次并缓存。

Server 只有在验证 raw v2 info-hash、固定 canonical 结构、已登记 BEP 52 root 与 piece-layer Merkle 结果全部一致后才缓存/发布 manifest；请求方在交给 libtorrent 写入之前还会独立再验证一次。

### 已实现的按需 Seed Session 生命周期

```text
ContentCatalog 确认文件存在
        |
Server lookup prepare / wakeup
        v
生成或加载 canonical v2 metadata
        |
canonical "content" 映射到真实本地文件
        |
以 seed_mode 隐藏 torrent 加入 libtorrent
        |
Server 在短 TTL 内标记该 Node ready
        |
请求方获取并验证 canonical manifest
        |
请求方加入隐藏 download torrent
        |
注入选定 TCP/uTP Peer
        |
空闲超时
        |
移除临时 canonical torrent
```

临时 torrent 被移除以后，ContentCatalog 仍然存在；这些隐藏 torrent 不进入正常 Task 持久化，也不进入普通任务 UI。

这才是真正实现：

```text
Task inactive
但 Content 仍可用
```

而不是让所有停止 torrent 永久常驻。

## 17. HTTP ResourceKey 提前发现与 Canonical WebSeed Hybrid

`ResourceKey` 仍然只是发现键，不是 ContentIdentity。Exact URL / validator key 只作为隐私保护 hint；raw URL 不上传公共目录，带 Cookie、凭据、自定义 Header、Referer/User-Agent override、URL credentials 或明显 signed/auth query 的请求都排除。

HTTP 的 WholeFile SHA-256 与 BitTorrent identity 必须分开理解。源站可以给 SHA-256，但这个 digest 不能反推出 BEP52 Merkle root，也不能反推出 BitTorrent v2 info-hash。OpenNet 必须通过 Content Directory 找到以前已经构建的 canonical metadata；找不到或验证失败时，`P2PPreferred` 就回落 aria2。

当前实现有两个显式 Task policy：

- `Aria2Only`：aria2 立即拥有 payload path，不做下载前 P2P discovery；
- `P2PPreferred`：优先尝试 canonical libtorrent；任一可信条件缺失时回落 aria2。

`P2PPreferred` 自动进入 canonical hybrid 必须同时满足：

1. caller 明确提供 WholeFile SHA-256；
2. candidate 中存在完全相同的 `WholeFileSha256` alias；
3. 已知 Content-Length 与 candidate size 一致；
4. candidate 有 BEP52 file-root identity；
5. output path 已知；
6. 请求是 public/privacy-safe；
7. 实际执行 `GET Range: bytes=0-0` 并得到 `206 Partial Content + Content-Range`。

`Accept-Ranges: bytes` 只作为 UI 提示，不足以启用 WebSeed。

### 当前可信主数据面

满足潜在条件时，aria2 先以 paused 状态创建，只承担现有 HTTP GID / SQLite / UI 的兼容 control shell；在 discovery 做出决定前它不拥有 payload 写入权。

```text
HTTP task
  +-- paused aria2 control shell（不写 payload）
  |
  v
canonical OpenNet.Content.v1 torrent
  +-- BEP19 HTTP URL Seed
  +-- OpenNet TCP/uTP Peer(s)
  |
  v
一个 libtorrent piece picker
  |
  v
一个 disk writer
```

final public HTTP origin 通过 `add_torrent_params::url_seeds` 交给 libtorrent。有有效 URL Seed 时，打开 canonical download 不再要求事先已经有 ready Peer。HTTP 与 P2P 都由同一个 libtorrent scheduler 和同一套密码学校验处理。

当前 `OpenNet.Content.v1/content` canonical layout 不需要为了 direct-file URL Seed 改协议。libtorrent 对完整单文件 URL（例如 `/file.bin`）会直接请求该 path；如果 URL 以 `/` 结尾，则它把这个 URL 当 base URL，并追加 torrent file path，形成 `/origin/OpenNet.Content.v1/content`。因此 OpenNet 自动注入 WebSeed 时只接受不以 `/` 结尾的 direct-file final URL；本地 deterministic HTTP Range tests 已把这两种行为写成断言。

当前 primary hybrid 由 libtorrent 直接写最终目标，因为 aria2 全程 paused。hybrid 失败时，OpenNet 先关闭隐藏 canonical session，删除 libtorrent 尚未完成的目标/控制残留，然后才 Resume aria2。这里是 writer ownership transfer，不是两个 writer 共存。

libtorrent 完成后，OpenNet 仍会重算整文件并要求 caller WholeFile SHA-256/size 完全一致，随后才把 HTTP record 标记 Complete 并写入 ContentCatalog。旧的 `.opennet-p2p-<gid>.part` 独立整文件 fallback 继续作为兼容/恢复路径保留。

Pause/Resume 现在会保留 active hidden canonical session：Pause 真正暂停 libtorrent session，同时 aria2 shell 继续保持 paused；Resume 恢复该 hidden session。Cancel/Remove/Delete 仍会关闭 hidden work 并 suppress late discovery，避免任务停止后被迟到 discovery 重新拉起。

### TransferCoordinator 不再是普通 HTTP P2SP 的默认方案

旧方案因为假定 aria2 与 libtorrent 是两个独立 writer，所以必须自己做 range ownership。现在普通 public HTTP 不需要这样做：BEP19 允许 libtorrent 在一个 torrent 内同时调度 HTTP URL Seed 与 BitTorrent Peer。

核心不变量变成：

```text
one physical output -> one active data writer
```

绝不能让 active aria2 与 libtorrent 同时写同一个物理文件。只有未来出现无法表达成 libtorrent URL Seed / Peer 的异构 source 时，才需要独立 `TransferCoordinator`。

## 18. Content Integrity 和传输加密不是一个问题

### Content Integrity

BEP 52 SHA-256 回答：

> 我收到的到底是不是目标文件的正确字节？

恶意 Peer 发错误数据，无法通过 Content Hash。

### Transport confidentiality / authentication

TLS、Noise、BitTorrent protocol encryption、未来后量子方案回答的是另外的问题：

- 旁路观察者能否读取流量；
- MITM 能否篡改 session；
- 对端身份是否经过认证。

当前架构刻意没有重新发明文件传输协议。第一版计划直接复用 libtorrent 的 peer protocol over TCP/uTP。

因此 TLS / Noise / PQC 的选择不会污染 ContentCatalog 或 Server Content schema。未来即使替换 transport security，已有 ContentIdentity 不需要迁移。

另外，BitTorrent MSE/PE 不应被等同理解为现代双向认证 TLS。以后如果 OpenNet 真正要求更强的 Peer 身份认证与机密性，应当在 transport/session boundary 上解决。

## 19. 与 BitComet 长效种子的兼容策略

BitComet 官方公开资料可以确认若干和本架构高度一致的宏观机制：

- LT-Seeding 独立于普通 BitTorrent upload；
- stopped task 仍然可以提供 LT-Seeding；
- 使用中心服务器查询 LT-Seed；
- 每个文件存在 160-bit LT-Seeding `filehash`；
- LT-Seeding 可以设置为 TCP、UDP 或同时使用；
- private torrent 会禁用 LT-Seeding。

官方资料：

- [BitComet Long-Term Seeding](https://wiki.bitcomet.com/long-term-seeding)
- [Inside BitComet](https://wiki.bitcomet.com/inside-bitcomet)

但公开资料不足以定义：

- LT `filehash` 的精确计算算法；
- LT Server 注册/查询 wire format；
- Peer handshake、framing、block request；
- 身份认证方式；
- BitComet 所谓 LT UDP 是否就是 uTP。

所以 OpenNet **绝不能假设**：

```text
BitComet LT UDP == uTP
```

当前 `BitCometLtSeed160Opaque` 的意义就是：先完整保存 20-byte compatibility identity，但不擅自把它解释成 SHA-1 或其他算法。

### 兼容性研究顺序

优先采用可复现的黑盒实验：

1. 制作严格控制内容的测试文件，让 BitComet 生成 torrent；
2. 提取 `filehash`，与 whole-file SHA-1 等候选算法比较；
3. 覆盖 16 KiB 边界、piece size 和 2 的幂附近长度；
4. 使用两台自己控制的 BitComet 客户端分别测试 TCP-only / UDP-only LT 模式并抓包；
5. 定位 Server discovery、handshake、file identifier、block framing、keepalive；
6. 只有当黑盒实验无法回答一个明确问题时，再针对该问题做窄范围静态/动态分析；
7. 所有结论都必须记录版本和可重复证据，不能把猜测写进 OpenNet 核心协议。

如果未来能够与 BitComet 官方合作，只需要实现 `BitCometCompatibilityAdapter`，不需要改掉 OpenNet 原生 ContentCatalog。

## 20. Server 威胁模型与后续安全加固

当前实现是 **alpha control plane**，还不能直接理解成已经适合完全不可信的公网部署。

已经具备：

- ContentIdentity 长度严格校验；
- 冲突 alias 拒绝；
- stale generation 拒绝；
- registration idempotency；
- 客户端不能随意指定公网 IP；
- lease 自动过期；
- 过期 Node 后台清理；
- private torrent 不注册；
- lookup 数量上限；
- inventory 数量上限；
- canonical 发布必须存在权威 BEP 52 file-root identity；
- Server 在发布前校验 canonical metainfo、info-hash 与 piece layer；
- 同一 Content 的冲突 canonical manifest/info-hash 会被拒绝；
- wakeup 具有有限生命周期、retry backoff 与短 seed-ready TTL。

公网正式部署前还需要：

1. Node key pair 与控制请求签名；
2. Server-issued 或 mutual-authenticated node session；
3. 和 requester / seeder / content 三者绑定的短生命周期 Peer Ticket；
4. per-node / per-IP rate limit 与 abuse quota；
5. Traversal 提供 endpoint ownership / reachability 证明；
6. 可信反向代理配置；
7. 比 RegistrationId 更完整的 replay protection；
8. inventory、lease churn、lookup、verification fail 的可观测性；
9. 使用正式 database migrations，替代开发阶段 `EnsureCreated`；
10. 对长期没有任何 presence 的 Content metadata 设置 retention policy。

## 21. 当前实现状态

### OpenNet 客户端已实现

- [x] multi-identity / multi-location ContentCatalog、后台 Hash 与启动校验；
- [x] BEP52 root 复用、canonical 1 MiB piece layer、deterministic `OpenNet.Content.v1` v2 swarm；
- [x] Content Directory inventory/lease、demand wakeup、manifest 验证与 TCP/uTP Peer 注入；
- [x] privacy-safe HTTP ResourceKey / ResourceObservation；
- [x] caller WholeFile SHA-256 + size gate；
- [x] 实际 206 + Content-Range 的 BEP19 eligibility probe；
- [x] hidden canonical download 接收 HTTP URL Seed；
- [x] primary canonical HTTP hybrid：paused aria2 shell + 单一 libtorrent writer；
- [x] HTTP URL Seed + OpenNet Peer 由同一个 libtorrent piece picker 调度；
- [x] hybrid progress 映射回现有 HTTP task UI / persistence；
- [x] caller SHA-256 二次校验、HTTP Complete 与 ContentCatalog 入库；
- [x] hybrid 失败后先清理 libtorrent 不完整目标，再把 ownership 交回 aria2；
- [x] 旧 separate-file peer fallback 继续保留用于兼容/恢复。

### OpenNet.Server 已实现

- [x] logical content / multi-identity / node presence / lease / generation / idempotency；
- [x] bounded lookup、demand wakeup、canonical manifest validator/cache；
- [x] ResourceObservation announce/lookup、digest-only key、TTL、candidate bound、stale ownership invalidation；
- [x] SQLite / MySQL wiring 与 service tests。

### 尚未实现 / 仍需加固

- [ ] deterministic HTTP Range + WebSeed + OpenNet Peer 端到端测试；
- [ ] 自动确认当前 canonical single-file layout 对应的 libtorrent WebSeed 真实请求路径；
- [ ] shutdown 时协作取消正在进行的 wakeup/lookup；
- [ ] Resume 后自动重新 discovery；
- [ ] redirect / Content-Disposition 晚到 filename 后启用 hybrid；
- [ ] 异常退出后的 stale hybrid/P2P partial 清理；
- [ ] same-target duplicate HTTP task 排他；
- [ ] HTTP task-shell cleanup/session persistence 的 crash/recovery 语义；
- [ ] Traversal verified IPv4/IPv6 candidate、hole punching、relay、Node key、Peer Ticket；
- [ ] production rate limit/migration 与 BitComet LT wire compatibility。

## 22. 下一步实现顺序

1. 增加 deterministic local HTTP Range server fixture，记录 libtorrent 对 `OpenNet.Content.v1` 的实际 WebSeed 请求路径。
2. 端到端覆盖 URL -> ResourceKey -> candidate -> wakeup -> manifest -> URL Seed + Peer -> BEP52 -> WholeFile SHA-256 -> HTTP Complete -> ContentCatalog。
3. 收紧 shutdown/cancel/resume、stale partial cleanup 与 same-target 排他。
4. 支持任务创建后才确定的 output filename。
5. 接 OpenNet.Traversal verified IPv4/IPv6 candidate 与 hole punching。
6. 公网部署前加入 Node key、请求签名、Peer Ticket、rate limit 与 production migrations。
7. BitComet compatibility 独立推进。

除非未来 source 无法表达成 libtorrent URL Seed / Peer，否则不要把通用 `TransferCoordinator` 重新作为 HTTP 主路线。
