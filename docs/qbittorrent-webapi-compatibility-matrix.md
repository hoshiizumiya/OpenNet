# qBittorrent WebAPI 2.16.0 兼容矩阵

基线 commit：`78bf5f0c715b447c4ca1127e3aae7cd3c2f0e90b`  
Action 总数：128  
强制 POST：80  
上游默认允许 GET/POST/HEAD：48

## 说明

方法列：

- `P`：上游明确限制为 POST；
- `R`：上游未限制，允许 GET、POST、HEAD。WebUI 通常使用 GET，但兼容层不能实现为 GET-only。

阶段列：

- M0：资源、认证和页面启动；
- M1：Torrent 基础闭环；
- M2：高级 Torrent 与完整设置；
- M3：RSS、搜索、日志、创建器和最终收口。

“现有基础”只描述设计开始时 OpenNet 能复用的底层能力，不表示 endpoint 已实现。

## auth

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `login` | P | M0 | 新增认证、限速、Cookie 会话 |
| `logout` | P | M0 | 新增会话销毁和过期 Cookie |

## app

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `buildInfo` | R | M0 | 编译期版本、依赖版本 |
| `cookies` | R | M2 | WebUI HTTP Cookie 存储 |
| `defaultSavePath` | R | M0 | `TorrentSettingsManager` |
| `deleteAPIKey` | P | M2 | WebUI auth repository |
| `getDirectoryContent` | R | M0 | 受限目录枚举，不能泄漏任意路径 |
| `getFreeSpaceAtPath` | R | M0 | 文件系统 free-space adapter |
| `networkInterfaceAddressList` | R | M2 | 系统网络接口 adapter |
| `networkInterfaceList` | R | M2 | 系统网络接口 adapter |
| `preferences` | R | M0→M2 | M0 启动键，M2 全量映射 |
| `processInfo` | R | M2 | 进程内存和启动时间 |
| `rotateAPIKey` | P | M2 | 生成、持久化并失效旧会话 |
| `sendTestEmail` | P | M2 | SMTP 设置服务；无能力时开发期 409 |
| `setCookies` | P | M2 | WebUI HTTP Cookie 存储 |
| `setPreferences` | P | M0→M2 | 事务化设置映射 |
| `shutdown` | P | M2 | 投递到应用生命周期，不能在 HTTP 线程直接退出 |
| `version` | R | M0 | 返回 OpenNet 版本字符串 |
| `webapiVersion` | R | M0 | 固定返回 `2.16.0` |

## clientdata

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `load` | R | M0 | SQLite JSON KV，支持 `keys` JSON 数组 |
| `store` | P | M0 | SQLite 原子批量写 |

## sync

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `maindata` | R | M0→M1 | M0 空快照；M1 per-session 完整差异 |
| `torrentPeers` | R | M1 | `TorrentPeerInfo` + per-session/per-torrent 差异 |

## torrents：读取和基础操作

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `count` | R | M1 | `GetTorrentCount()` |
| `info` | R | M1 | 新批量快照和 Qbt serializer |
| `properties` | R | M1 | 扩展 `TorrentDetailInfo` |
| `trackers` | R | M1 | 现有 Tracker 详情需补齐字段和状态整数 |
| `webseeds` | R | M1 | 新增 URL seed 快照 |
| `files` | R | M1 | 现有文件详情需补 piece range/is_seed |
| `pieceHashes` | R | M2 | torrent_info piece hashes |
| `pieceStates` | R | M1 | handle status/piece bitfield |
| `pieceAvailability` | R | M1 | piece availability |
| `add` | P | M1 | `AddMagnet`/`AddTorrentFile`，补 multipart 选项 |
| `start` | P | M1 | `ResumeTorrent` |
| `stop` | P | M1 | `PauseTorrent`，保持用户停止语义 |
| `delete` | P | M1 | `RemoveTorrent(deleteFiles)` |
| `recheck` | P | M1 | `ForceRecheck` |
| `reannounce` | P | M1 | 新增 `force_reannounce()` |
| `filePrio` | P | M1 | `SetFilePriorities`，补索引选择 |
| `downloadFile` | R | M2 | 安全流式读取已下载文件/文件范围 |
| `export` | R | M2 | 导出 `.torrent` |
| `fetchMetadata` | P | M2 | 复用 `TorrentMetadataFetcher`，新增任务隔离 |
| `parseMetadata` | P | M2 | bencode 解析和安全限额 |
| `saveMetadata` | R | M2 | 输出已抓取元数据 |

## torrents：Tracker、WebSeed 和 Peer

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `addTrackers` | P | M2 | handle tracker mutation |
| `editTracker` | P | M2 | handle tracker mutation |
| `removeTrackers` | P | M2 | handle tracker mutation |
| `addWebSeeds` | P | M2 | URL seed mutation |
| `editWebSeed` | P | M2 | URL seed mutation |
| `removeWebSeeds` | P | M2 | URL seed mutation |
| `addPeers` | P | M2 | `connect_peer`，校验地址列表 |

## torrents：限速、队列和模式

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `uploadLimit` | R | M1 | per-torrent upload limit |
| `downloadLimit` | R | M1 | per-torrent download limit |
| `setUploadLimit` | P | M1 | per-torrent upload limit |
| `setDownloadLimit` | P | M1 | per-torrent download limit |
| `setShareLimits` | P | M2 | ratio/seeding/inactive limits |
| `increasePrio` | P | M2 | queue position |
| `decreasePrio` | P | M2 | queue position |
| `topPrio` | P | M2 | queue top |
| `bottomPrio` | P | M2 | queue bottom |
| `setAutoManagement` | P | M2 | auto-managed flag + persistence |
| `setForceStart` | P | M2 | auto-managed/paused combination |
| `setSuperSeeding` | P | M2 | super-seeding flag |
| `toggleSequentialDownload` | P | M2 | sequential flag |
| `toggleFirstLastPiecePrio` | P | M2 | first/last piece priority |

## torrents：路径、命名和 TLS

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `setLocation` | P | M2 | 移动存储并返回异步语义 |
| `setSavePath` | P | M2 | save path + persistence |
| `setDownloadPath` | P | M2 | incomplete path + persistence |
| `rename` | P | M2 | Torrent display name |
| `setComment` | P | M2 | 用户 comment |
| `renameFile` | P | M2 | 文件映射和磁盘一致性 |
| `renameFolder` | P | M2 | 批量文件映射事务 |
| `SSLParameters` | R | M2 | SSL torrent certificate/key |
| `setSSLParameters` | P | M2 | 安全凭据存储 |

## torrents：分类与标签

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `categories` | R | M2 | Category repository |
| `createCategory` | P | M2 | Category repository |
| `editCategory` | P | M2 | Category repository + 任务更新 |
| `removeCategories` | P | M2 | Category repository + 清除引用 |
| `setCategory` | P | M2 | Torrent-category relation |
| `tags` | R | M2 | Tag repository |
| `createTags` | P | M2 | Tag repository |
| `deleteTags` | P | M2 | Tag repository + 清除引用 |
| `addTags` | P | M2 | 多对多 relation |
| `setTags` | P | M2 | 替换多对多 relation |
| `removeTags` | P | M2 | 多对多 relation |

## transfer

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `info` | R | M1 | `SessionStats`，补连接状态和限速 |
| `uploadLimit` | R | M1 | libtorrent settings |
| `downloadLimit` | R | M1 | libtorrent settings |
| `setUploadLimit` | P | M1 | `ApplySettings` |
| `setDownloadLimit` | P | M1 | `ApplySettings` |
| `getSpeedLimits` | R | M2 | 普通/备用限速设置 |
| `setSpeedLimits` | P | M2 | 普通/备用限速设置 |
| `speedLimitsMode` | R | M2 | WebUI speed mode |
| `setSpeedLimitsMode` | P | M2 | WebUI speed mode |
| `toggleSpeedLimitsMode` | P | M1 | 切换并触发 sync |
| `banPeers` | P | M2 | IP filter/peer ban service |

## log

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `main` | R | M3 | 有界结构化 ring buffer |
| `peers` | R | M3 | peer connection log ring buffer |

## rss

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `addFeed` | P | M3 | 复用 `RSSManager` |
| `addFolder` | P | M3 | 扩展 RSS 树 |
| `items` | R | M3 | RSS 树 serializer |
| `markAsRead` | P | M3 | `RSSDatabase` |
| `moveItem` | P | M3 | RSS 树事务 |
| `refreshItem` | P | M3 | `RSSManager` refresh |
| `removeItem` | P | M3 | RSS 树事务 |
| `setFeedURL` | P | M3 | `RSSManager` |
| `setFeedRefreshInterval` | R | M3 | 注意上游未限制为 POST |
| `cloneRule` | P | M3 | RSS 自动下载规则 |
| `matchingArticles` | R | M3 | 规则匹配器 |
| `removeRule` | P | M3 | 规则 repository |
| `renameRule` | P | M3 | 规则 repository |
| `rules` | R | M3 | 规则 serializer |
| `setRule` | P | M3 | 规则 repository + validator |

## search

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `start` | P | M3 | 隔离的搜索 worker |
| `stop` | P | M3 | worker cancellation |
| `status` | R | M3 | 搜索任务 repository |
| `results` | R | M3 | 分页结果 serializer |
| `delete` | P | M3 | 清理任务和 worker |
| `downloadTorrent` | R | M3 | 安全代理下载 |
| `plugins` | R | M3 | 插件清单 |
| `installPlugin` | P | M3 | 签名/来源校验与沙箱 |
| `uninstallPlugin` | P | M3 | 插件生命周期 |
| `enablePlugin` | P | M3 | 插件生命周期 |
| `updatePlugins` | P | M3 | 插件更新 |

## torrentcreator

| Action | 方法 | 阶段 | 现有基础/设计映射 |
| --- | --- | --- | --- |
| `addTask` | P | M3 | libtorrent create_torrent worker |
| `status` | R | M3 | 创建任务进度 |
| `torrentFile` | R | M3 | 结果流式下载 |
| `deleteTask` | P | M3 | 取消或清理任务 |

## 关键响应对象

### `sync/maindata`

顶层字段：

```text
rid, full_update,
torrents, torrents_removed,
categories, categories_removed,
tags, tags_removed,
trackers, trackers_removed,
server_state
```

`server_state` 至少包含：

```text
alltime_dl, alltime_ul, average_time_queue, connection_status,
dht_nodes, dl_info_data, dl_info_speed, dl_rate_limit,
free_space_on_disk, global_ratio,
last_external_address_v4, last_external_address_v6,
queueing, queued_io_jobs, queued_tracker_announces,
read_cache_hits, read_cache_overload, refresh_interval,
request_latency, total_buffers_size, total_peer_connections,
total_queued_size, total_wasted_session,
up_info_data, up_info_speed, up_rate_limit,
use_alt_speed_limits, write_cache_overload
```

### `sync/torrentPeers`

顶层字段：

```text
rid, full_update, show_flags, peers, peers_removed
```

Peer 字段：

```text
client, peer_id_client, connection, country, country_code,
dl_speed, files, flags, flags_desc, host_name, ip, i2p_dest,
port, progress, relevance, contribution, downloaded, uploaded,
up_speed
```

### `torrents/files`

每项字段：

```text
index, name, size, progress, priority, piece_range
```

第一项额外带 `is_seed`，无元数据时返回空数组。

### `torrents/trackers`

每项字段：

```text
url, status, tier, num_peers, num_seeds, num_leeches,
num_downloaded, msg
```

必须保留 qBittorrent 的特殊 pseudo tracker 行和 tracker 状态整数。

## CI 完整性检查

CI 从固定 qBittorrent 源码中的十个 Controller header 提取 `*Action()`，从 `webapplication.h` 提取强制 POST 表，然后与本文件对应的机器可读 manifest 比较。以下变化必须阻断升级：

- action 新增、删除或改名；
- 强制 POST 集合变化；
- `API_VERSION` 变化；
- Torrent 状态枚举变化；
- `sync` 顶层字段或 `_removed` 规则变化；
- 原版 WebUI 出现 manifest 之外的新 `/api/v2` 调用。

人工维护的 Markdown 用于设计审查；机器可读 manifest 才是 CI 的唯一 endpoint 真值。
