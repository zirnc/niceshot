# niceshot：架构设计与学习路线

> 工作名 `niceshot`（截图架），可随时改。目标不是复刻 Tendedero，而是借它练"软件架构"这件事：
> 先做一个能用的 C++ CLI，再把同一套内核接到 macOS UI 上。
>
> 适用前提：会一点 C++，没有软件工程经验；每周能投入 8–12 小时。

---

## 0. 先立三条规矩

1. **先跑通，再抽象。** 第一个版本允许"丑但正确"，架构是演化出来的，不是第一天画出来的。
2. **依赖只能指向内核。** 内核（domain + app + ports）不知道 SQLite、FSEvents、CLI、SwiftUI 的存在。
3. **不能独立测试的边界，通常是错的。** 每加一个模块，先问：我能不能不启动 App、不连数据库就测它？

这三条会在后面反复出现。architecture（架构）不是文件夹长什么样，而是**依赖方向**和**变化被关在哪里**。

### 如果只记五件事

1. **内核 = `domain`（纯规则）+ `ports`（接口）+ `app`（用例）**，它不知道 SQLite、macOS、CLI。
2. **CLI 是组合根**：只有它 new 具体实现；`main.cpp` 只做解析、组装、渲染。
3. **四个端口就够**：`ScreenshotSource`、`ItemRepository`、`ImageToolkit`、`Clock`。
4. **先写测试替身**：内存 repo、假 source、假时钟；用例能在没有文件系统的情况下测。
5. **v0.1 只做 CLI**，UI 是第二个前端；数据模型稳定之前不要接 UI。

---

## 1. v0.1 的目标：只做 CLI

### 1.1 要有的命令

```text
niceshot scan   [--dir PATH] [--json] [--verbose]
niceshot list   [--limit N] [--json]
niceshot thumb  <id> [--size 512] --out FILE
niceshot watch  [--json]
niceshot prune  [--days N] [--apply]
niceshot config get|set <key> [value]
niceshot version
```

### 1.2 明确不做（v0.1）

- 不做 UI、不做菜单栏、不做缩略图悬浮窗；
- 不做删除/移动/标记/Quick Look；
- 不做录屏、OCR、标签、搜索；
- 不做多目录递归监听（先只监听一个目录）；
- 不做登录启动、签名、公证、自动更新。

**范围冻结**是第一个工程能力。v0.1 的唯一验收标准：在真实 macOS 上，`scan` 能把截图索引进 SQLite，`list` 能按时间倒序列出，`thumb` 能导出 PNG，`watch` 能持续打印新截图，`prune` 能按天数清理索引，全程有测试、有 README、有 Git 历史。

### 1.3 为什么先做 CLI

- 它强迫你把"业务规则"和"界面"分开；
- 它能被脚本和测试驱动，反馈快；
- 它以后仍然是调试工具：UI 出问题时，用 CLI 验证内核是否正常；
- 它是未来 UI 的"第一个前端"，证明内核真的与前端无关。

---

## 2. 架构总览

### 2.1 一张图

```text
        ┌─────────────────────┐        ┌──────────────────────────┐
        │  CLI（v0.1）         │        │  macOS UI（以后）         │
        │  参数解析 / 文本 / JSON│        │  SwiftUI + AppKit 面板    │
        └──────────┬──────────┘        └────────────┬─────────────┘
                   │                                 │ C API（不透明句柄）
                   └──────────────┬──────────────────┘
                                  ▼
                 ┌─────────────────────────────────────┐
                 │  niceshot_core（C++ 静态库）        │
                 │                                     │
                 │  app/      用例：scan/list/thumb/…   │
                 │  domain/   值对象 + 规则（纯 C++）    │
                 │  ports/    内核需要的接口（抽象）     │
                 └──────────────▲──────────────────────┘
                                │ 实现（implements）
                 ┌──────────────┴──────────────────────┐
                 │  adapters/                          │
                 │  sqlite / FSEvents / ImageIO /      │
                 │  CFPreferences / 文件系统            │
                 └─────────────────────────────────────┘
```

- **CLI 是组合根（composition root）**：只有它知道所有具体类型，负责 new 出适配器、塞进用例。
- **UI 以后也是组合根**：通过 C API 拿到同一个内核，不重新实现规则。
- **内核不 include 任何 macOS 头文件**（除了 ports 里允许出现标准库类型；平台类型一律不出现）。

### 2.2 分层职责

| 层 | 目录 | 知道什么 | 不知道什么 | 怎么测 |
|---|---|---|---|---|
| 领域 | `src/domain/` | `Item`、时间、像素尺寸、保留策略 | 文件系统、SQL、macOS | 纯单元测试，毫秒级 |
| 端口 | `src/ports/` | 内核需要哪些能力（抽象类） | 谁来实现 | 编译期检查 |
| 应用 | `src/app/` | 用例流程、事务边界、错误汇总 | CLI 输出、SQL 语法、AppKit | 用假实现测 |
| 适配器 | `src/adapters/` | SQLite、FSEvents、ImageIO、plist | 业务规则 | 临时目录 + 真实依赖 |
| 前端 | `src/cli/` | 参数、输出格式、退出码 | 业务规则 | 端到端跑二进制 |

### 2.3 依赖规则（必须能背下来）

```text
cli  ──▶ adapters ──▶ core
                  └──▶ core（ports 由 adapters 实现，但编译依赖仍指向 core）
core ──▶ 标准库（仅此而已）
```

- `domain` 和 `app` 不允许 `#include <sqlite3.h>`、`<CoreServices/CoreServices.h>`、`<ImageIO/ImageIO.h>`。
- `adapters` 可以 include 平台头，也可以 include `core` 的头。
- `core` 反过来 include `adapters` 就是架构塌了，编译会立刻发现（因为 core target 不链接 adapters）。

---

## 3. 目录结构

```text
niceshot/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── docs/
│   ├── ARCHITECTURE.md          # 本文件
│   └── adr/                     # 架构决策记录（见 §15）
│       ├── 0001-cli-first.md
│       ├── 0002-sqlite-as-index.md
│       └── 0003-ports-only-where-needed.md
├── src/
│   ├── domain/
│   │   ├── item.h / item.cpp            # Item、ItemId、Kind
│   │   ├── retention.h / retention.cpp  # 保留策略（纯函数）
│   │   └── report.h                     # ScanReport / PruneReport
│   ├── ports/
│   │   ├── screenshot_source.h          # 发现 + 监听
│   │   ├── item_repository.h            # 持久化
│   │   ├── image_toolkit.h              # 探尺寸 + 缩略图
│   │   └── clock.h                      # 可替换的"现在"
│   ├── app/
│   │   ├── scan_library.h / .cpp
│   │   ├── list_items.h / .cpp
│   │   ├── make_thumbnail.h / .cpp
│   │   ├── watch_library.h / .cpp
│   │   └── prune_library.h / .cpp
│   ├── adapters/
│   │   ├── macos/
│   │   │   ├── screenshot_folder.h / .cpp   # CFPreferences 读截图目录
│   │   │   ├── vnode_source.h / .cpp         # dispatch source 监听
│   │   │   └── imageio_toolkit.h / .cpp      # ImageIO 探尺寸/缩略图
│   │   ├── sqlite/
│   │   │   ├── sqlite_repository.h / .cpp
│   │   │   └── migrations.cpp
│   │   └── config/
│   │       └── json_config.h / .cpp
│   └── cli/
│       ├── main.cpp                 # 组合根：解析 → 组装 → 执行 → 输出
│       ├── args.h / .cpp            # 只做参数解析
│       ├── output.h / .cpp          # 文本 / JSON 渲染
│       └── exit_codes.h
├── tests/
│   ├── domain/retention_test.cpp
│   ├── app/scan_library_test.cpp    # 用假实现
│   ├── adapters/sqlite_test.cpp     # 临时数据库
│   ├── cli/cli_test.sh              # 端到端（CTest 调用）
│   └── support/
│       ├── fake_source.h
│       ├── in_memory_repository.h
│       ├── fake_image_toolkit.h
│       ├── fake_clock.h
│       └── temp_dir.h
└── third_party/
    ├── doctest.h
    └── json.hpp                     # nlohmann/json，单头文件
```

**命名原则**：目录名说"做什么"（scan、list、thumb），不是"用什么框架"（sqlite、macos）。这是 Clean Architecture 里说的 screaming architecture——看目录就知道产品在干什么。

---

## 4. 领域模型（domain）

### 4.1 核心类型

| 类型 | 内容 | 约束 |
|---|---|---|
| `ItemId` | 字符串（路径的稳定哈希） | 非空、稳定、可打印 |
| `Item` | id、绝对路径、拍摄时间、像素尺寸、类型（截图/图片）、收藏标记 | 路径非空、尺寸 > 0、时间有效 |
| `Kind` | `Screenshot` / `Image` / `Recording`（v0.1 只用前两个） | — |
| `RetentionPolicy` | 保留天数 | 0 表示不清理 |
| `ScanReport` | discovered / added / updated / skipped / failed | 计数不为负 |
| `PruneReport` | candidates / removed / freed | 同上 |

**规则**：domain 里只出现标准库类型（`std::string`、`std::chrono::sys_seconds`、`std::optional`、`std::vector`）。不出现 `CGImageRef`、`sqlite3*`、`NSURL`。

### 4.2 为什么 domain 要独立

- 它变化最慢，是内核里最稳定的部分；
- 它能在没有文件系统、没有数据库的机器上测；
- 它让"业务规则"可以被人读懂，而不是埋在 SQL 和 UI 回调里。

### 4.3 一个容易犯的错

不要在 `Item` 上加 `save()`、`load()`、`toJson()`。**领域对象只表达"是什么"，不表达"存哪里、怎么显示"。** 序列化是适配器的事，格式化是 CLI 的事。

---

## 5. 端口（ports）：内核需要的四件事

端口就是"内核向外界提出的要求"，用抽象类表达。**只抽象真正会变、或需要假实现的东西**，不要给每个类都配接口。

### 5.1 `ScreenshotSource`

```text
discover() -> vector<Candidate>          // 一次性扫描
watch(callback<Candidate>) -> Handle     // 持续监听，可取消
```

- `Candidate` 只含路径（以及可选的发现时间），不含尺寸、不含数据库 id。
- 实现：macOS 的 `CFPreferences` 读目录 + 目录枚举；监听用 dispatch vnode source。
- 为什么抽象：测试时用 `FakeSource` 直接喂候选；以后换 FSEvents 或换平台不用改用例。

### 5.2 `ItemRepository`

```text
upsertMany(vector<Item>)                 // 一个事务内批量写入
findById(ItemId) -> optional<Item>
findByPath(path) -> optional<Item>
list(ListQuery) -> vector<Item>          // 按时间倒序、limit
remove(ItemId)                           // 只删索引，不删文件
count() -> size_t
```

- `upsertMany` 而不是 `beginTransaction()`：**接口按用例的需要设计，不要按数据库的能力设计。** 事务是 SQLite 的实现细节。
- 为什么抽象：单元测试用内存实现；以后换 Postgres/文件存储也不用改用例。

### 5.3 `ImageToolkit`

```text
probe(path) -> optional<ImageInfo>       // 宽、高、类型；不 decode 全图
thumbnail(path, maxPx) -> optional<Thumbnail>  // PNG 字节或 RGBA
```

- 合并"探尺寸"和"出缩略图"两件事，因为它们共享同一个技术（ImageIO），且总是一起变化。
- 为什么抽象：测试时不依赖真实图片解码；以后加 HEIC/视频也有统一入口。

### 5.4 `Clock`

```text
now() -> sys_seconds
```

- 只为了测试保留策略：假时钟能让你测"30 天后"而不用真的等。
- 这是"可测试性驱动设计"的最小例子。

### 5.5 暂时不要的端口

`FileSystem`、`Logger`、`Settings`、`EventBus`——v0.1 用具体类或参数即可。**等第二个实现或第二个测试需要它时再抽象**（rule of three）。

---

## 6. 用例（app）：一个用例 = 一个用户意图

| 用例 | 输入 | 输出 | 副作用 |
|---|---|---|---|
| `ScanLibrary` | source、repo、imageToolkit、clock | `ScanReport` | 写数据库 |
| `ListItems` | repo、query | `vector<Item>` | 无 |
| `MakeThumbnail` | repo、imageToolkit、id、size | `Thumbnail` | 写缓存文件 |
| `WatchLibrary` | source、scanUseCase、eventSink | 运行直到取消 | 持续写库、发事件 |
| `PruneLibrary` | repo、policy、clock、dryRun | `PruneReport` | 删索引/缩略图 |

### 6.1 `scan` 的数据流

```text
CLI 解析参数
  → 组装 Source / Repository / ImageToolkit / Clock
  → ScanLibrary::execute()
      1. source.discover()                    拿到候选路径
      2. 过滤：扩展名、xattr（是否截图）、是否已索引
      3. 对每个新文件：
           a. 等文件稳定（大小/修改时间连续两次一致）
           b. imageToolkit.probe()             读宽高
           c. 组装 Item（id、路径、拍摄时间）
      4. repo.upsertMany(items)                一个事务
      5. 汇总 ScanReport（成功/跳过/失败及原因）
  → CLI 渲染：人读文本 或 JSON
  → 退出码：0 成功 / 3 部分失败 / 4 权限
```

这个流程里，**CLI 不知道 SQL，SQLite 不知道 xattr，ImageIO 不知道数据库**。每一层只做一件事。

### 6.2 用例的写法约定

- 构造函数注入依赖，`execute()` 只做一件事；
- 不打印、不读环境变量、不解析参数；
- 失败用返回值（`Result<T>`），不要用异常做控制流；
- 返回值是"报告"，让前端决定怎么显示；
- 一个用例对应一个文件，文件不超过 ~200 行。

---

## 7. 持久化设计

### 7.1 谁是真源

**文件系统是真源，数据库是索引/缓存。** 文件被用户删了、移走了，数据库不能"坚持"它还在。所以：

- `scan` 每次都做对账：DB 里有、磁盘上没有 → 标记 `missing` 或删除索引；
- 缩略图是**派生数据**，可以随时删、随时重建，不进备份；
- 数据库损坏时，最坏情况是重新 `scan` 一遍，不丢原图。

这是"single source of truth"的典型应用，也是很多人做截图管理器时踩的坑。

### 7.2 表结构（v1）

```sql
PRAGMA user_version = 1;

CREATE TABLE items (
  id           TEXT PRIMARY KEY,
  path         TEXT NOT NULL UNIQUE,
  captured_at  INTEGER NOT NULL,   -- Unix epoch 秒，UTC
  width        INTEGER NOT NULL,
  height       INTEGER NOT NULL,
  kind         TEXT NOT NULL,      -- 'screenshot' | 'image'
  favorite     INTEGER NOT NULL DEFAULT 0,
  indexed_at   INTEGER NOT NULL,
  missing      INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX idx_items_captured ON items(captured_at DESC);
```

- 时间统一存 UTC epoch，显示时再转本地时区；不要存格式化字符串。
- 路径存绝对路径；`UNIQUE` 防止重复索引。
- v0.1 不做内容 hash；v0.2 加 `content_hash` 时用迁移（见 7.4），这正好练"schema 演进"。
- 缩略图不放 BLOB，放 `~/Library/Application Support/niceshot/thumbs/<id>-<size>.png`；数据库只存元数据。

### 7.3 SQLite 使用规则

- 打开时：`PRAGMA journal_mode=WAL; PRAGMA foreign_keys=ON; PRAGMA busy_timeout=5000;`
- 所有 SQL 用 prepared statement + 绑定参数，**永远不要拼字符串**；
- 批量写入包在一个事务里；
- 读多写少的场景，WAL 足够；
- 连接不跨线程共享（v0.1 单线程，简单）。

### 7.4 迁移

```text
PRAGMA user_version 读出当前版本
if version < 1: 建表，设为 1
if version < 2: ALTER TABLE items ADD COLUMN content_hash TEXT，设为 2
```

规则：**已发布的迁移只增不改**；迁移函数只依赖 SQL，不依赖 domain 类型；每次迁移写一个集成测试（从空库升到最新）。

---

## 8. CLI 设计

### 8.1 命令与输出

| 命令 | 人读输出（stdout） | `--json` | 备注 |
|---|---|---|---|
| `scan` | `added 3, skipped 12, failed 0` | `{"discovered":15,"added":3,...}` | 幂等 |
| `list` | 每行 `时间  尺寸  路径` | 数组 | 默认 20 条 |
| `thumb` | `wrote /path/out.png` | `{"path":...}` | 不存在返回 3 |
| `watch` | 每行 `added <path>` | JSONL，一行一个事件 | Ctrl-C 退出 |
| `prune` | `would remove 5` / `removed 5` | 报告对象 | 默认只报告，`--apply` 才执行 |
| `config` | `key = value` | 对象 | get/set |

### 8.2 约定

- **stdout 只放结果，stderr 放日志**；这样 `niceshot list --json | jq` 永远干净。
- 退出码固定：`0` 成功、`2` 用法错误、`3` 找不到、`4` 权限、`5` 存储错误。
- `--data-dir` 用于测试隔离；默认 `~/Library/Application Support/niceshot`。
- 配置优先级：命令行 > 环境变量 `NICESHOT_*` > 配置文件 > 默认值。写进 README。
- 破坏性操作默认安全：`prune` 默认只报告（dry-run），要真删必须显式 `--apply`。
- 参数解析先手写（100 行以内），不要一上来引入 CLI 库；手写一遍才懂 argv、`--`、`-h`。

### 8.3 组合根长什么样

`main.cpp` 只做四件事：

```text
parse(argv) -> Options
buildDependencies(options) -> { repo, source, toolkit, clock }
dispatch(command, deps) -> Result<Report>
render(report, options.json) -> 输出 + 退出码
```

业务逻辑一行都不许写在 `main.cpp`。**`main.cpp` 超过 200 行就是架构警报。**

---

## 9. 横切关注点

### 9.1 错误处理

- 用 `std::expected<T, Error>`（C++23，你机器上的 clang 21 已支持）；
- `Error { code, message, path? }`，code 是枚举（NotFound / Permission / Io / Corrupt / Unsupported）；
- 预期内的失败（文件不在、没权限）用返回值；异常只留给"程序 bug"（越界、断言失败）；
- 适配器把 `errno` / `OSStatus` 翻译成 `Error`，不要把系统错误码泄漏到用例；
- 每个边界都加一点上下文（哪个路径、哪个命令），但不要重复打日志。

### 9.2 日志

- v0.1 用 `stderr` + 级别（error/warn/info/debug），`--verbose` 打开 debug；
- 日志格式固定：`[level] message key=value`，方便以后 grep；
- 内核不直接 `std::cout`；需要日志时通过参数传入回调或简单 `Logger` 接口；
- 不要在日志里打图片内容、用户路径以外的隐私。

### 9.3 时间与路径

- 时间：`std::chrono::sys_seconds`，存 UTC；显示用 `localtime_r` + `strftime`（比 `std::chrono` 时区支持更稳，macOS 上够用）。
- 路径：`std::filesystem::path`；输出统一绝对路径；macOS 文件名是 UTF-8，可能是 NFD 规范化，比较时不要假设 NFC。
- 符号链接：v0.1 不跟随，`symlink_status` 判断。

### 9.4 线程

- v0.1 全部单线程；`watch` 用 dispatch queue + 信号量等待，回调里只做入队；
- 内核类默认**不线程安全**，在头文件注释里写清楚；
- 以后 UI 接入时，由 Swift 侧保证主线程，内核用一条串行队列；
- 不要为了"看起来专业"提前上线程池。

### 9.5 macOS 适配要点（v0.1 会直接用到）

- **截图目录**：先读 `com.apple.screencapture` 的 `location-screenshot`（macOS 27），回退 `location`（更早版本），再回退 `~/Desktop`。三个都要处理，否则在新系统上会静默扫错目录。
- **是不是截图**：桌面上的图片看扩展属性 `com.apple.metadata:kMDItemIsScreenCapture`；自建目录里可以放宽为"所有图片"。
- **创建时间**：`stat` 的 `st_birthtimespec`（macOS 专有），不要用 `last_write_time` 代替拍摄时间。
- **尺寸**：`CGImageSourceCopyPropertiesAtIndex` 只读元数据，不要为了拿宽高 decode 整张图。

### 9.6 权限（macOS 特有）

- 读桌面会触发 TCC 提示；**CLI 继承的是终端 App 的权限**（Terminal/iTerm 需要在"隐私与安全性 → 文件与文件夹"里被授权），这点和最终 .app 不一样，测试时要知道。
- 首次 `open()` 可能阻塞等待授权：不要在 UI 主线程做；CLI 里至少给一句提示。
- 被拒绝时返回 `Permission` 错误，给出可操作的提示（"到系统设置里给终端开桌面访问"）。

---

## 10. 测试策略

| 层 | 测什么 | 怎么测 | 数量 |
|---|---|---|---|
| domain | 保留策略、去重规则、边界值 | 纯函数 + 假时钟 | 多而快 |
| app | 用例流程、错误汇总、幂等 | 假 source / 内存 repo / 假 toolkit | 中 |
| adapters | SQL 正确、迁移、文件监听 | 临时目录 + 真 SQLite | 少而慢 |
| cli | 参数、退出码、JSON 形状 | CTest 跑二进制 + 比对 | 少量 |

### 10.1 假实现怎么写

- `FakeSource`：构造时给一组候选路径，`discover()` 返回它们；`watch()` 手动触发回调。
- `InMemoryRepository`：`std::map<ItemId, Item>`，实现同一接口。
- `FakeImageToolkit`：按路径返回固定尺寸/固定 PNG 字节。
- `FakeClock`：可设置 `now`，用来测"31 天后应删除"。

**这些假实现本身就是架构的验收工具**：如果用例没法用假实现测，说明它偷偷依赖了具体技术。

### 10.2 测试金字塔

- 70% domain/app（毫秒级）；
- 20% adapters（临时目录，秒级）；
- 10% CLI 端到端（进程级）。
- 不追求覆盖率数字；追求"每个 bug 先有一个失败测试"。

### 10.3 工具

- doctest（单头文件，编译快）；
- CTest 统一跑；
- Debug 构建默认开 ASan + UBSan；
- `watch` 稳定后加 TSAN 跑一夜；
- 格式化用 `clang-format`，静态检查用 `clang-tidy`（先只开少量检查，别被淹）。

---

## 11. 构建

你这台机器：clang 21、SQLite 3.54、ImageIO/CoreServices 都在；CMake 4.4.4 已随 Homebrew 装好（`/opt/homebrew/bin/cmake`），Ninja 可选。

如果终端里 `cmake` 找不到，把 `/opt/homebrew/bin` 加进 `PATH`，或直接用全路径。

### 11.1 CMake 目标

```text
niceshot_core       STATIC  domain + app + ports（不链接任何框架）
niceshot_adapters   STATIC  sqlite + macOS + config（链接 core）
niceshot            EXE     cli（链接 adapters）
niceshot_tests      EXE     doctest（链接 core + adapters）
```

关键点：

- `target_include_directories(niceshot_core PUBLIC src)`，其他 target 只 include `core` 暴露的头；
- `niceshot_core` **不**链接 `sqlite3`、`CoreServices`、`ImageIO`——链接错了就说明依赖方向错了；
- CMake 选项 `NICESHOT_SANITIZE=ON` 时加 `-fsanitize=address,undefined`；
- 编译选项：`-std=c++23 -Wall -Wextra -Wpedantic -Wshadow`（先不要 `-Werror`，会被警告劝退）；
- `enable_testing()` + `add_test()` 跑单元测试和 CLI 端到端。

### 11.2 依赖选择

| 依赖 | 来源 | 理由 |
|---|---|---|
| SQLite | 系统 `-lsqlite3`（3.54） | 零安装、稳定；以后需要新版本再 vendor amalgamation |
| ImageIO / CoreServices / CoreFoundation | 系统 framework | 缩略图、FSEvents、偏好设置 |
| doctest | vendor 单头文件 | 轻、快、无构建负担 |
| nlohmann/json | vendor 单头文件 | 配置 + JSON 输出；避免手写转义 |

**先不要上 vcpkg/Conan**：这个项目依赖少，包管理器会引入另一套要学的东西。等依赖超过 5 个再说。

### 11.3 常用命令

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNICESHOT_SANITIZE=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/niceshot --help
```

---

## 12. 演进到 UI（先别做，但 architecture 要留口）

### 12.1 为什么 CLI 能长成 UI

因为 UI 只是第二个前端：它调用同一批用例，订阅同一批事件。需要新增的只有：

- 一个 C API 头（`extern "C"`，不透明句柄）；
- 一个事件回调；
- 平台动作端口（删除到废纸篓、打开、Quick Look、Markup）。

### 12.2 C API 的规则

```text
typedef struct td_library td_library;
td_library* td_open(const char* data_dir, td_error* err);
void        td_close(td_library*);
int         td_scan(td_library*, td_scan_report* out);
size_t      td_list(td_library*, td_query, td_item* out, size_t cap);
CGImageRef  td_thumbnail(td_library*, const char* id, int size);  // caller releases
void        td_set_event_cb(td_library*, td_event_cb cb, void* ctx);
```

- 不透明句柄，调用方不知道内部布局；
- 不暴露 `std::string` / `std::vector` / 异常；
- 错误用出参 + 错误码；
- 所有权写清楚：谁分配、谁释放；
- 线程规则写清楚：哪些函数必须在主线程、回调在哪个队列；
- `CGImageRef` 是 C 类型，可以安全跨边界，Swift 侧直接包成 `Image`。

### 12.3 UI 侧的边界

- SwiftUI 只做展示和交互，不写业务规则；
- AppKit 负责 `NSPanel`、状态栏、拖拽、Quick Look；
- Swift 侧有一个 `LibraryViewModel`：调用 C API、持有快照、把事件转成 `@Published`；
- 删除/移动等动作通过 `FileActions` 端口注入，内核不直接调 `NSFileManager`。

### 12.4 什么时候开始 UI

当 CLI 满足：`scan`/`list`/`thumb`/`watch`/`prune` 稳定两周、测试通过、你不再想改数据模型。**UI 会放大数据模型的错误**，别太早接。

---

## 13. 学习路线（按阶段，不按技术栈）

> 原则：30% 读，70% 写。每个阶段都有"能演示的产物"，不是纯读书。

### 阶段 0：工具与工作流（第 1 周）

**学**：终端、Git（init/add/commit/diff/log/branch）、CMake 最小工程、编译器警告、lldb 断点、ASan。

**做**：`niceshot --help` + `version` + 一个测试 + CMake + Git 首次提交。

**验收**：能在干净目录里三条命令构建并跑测试；能解释 `git status` 每一行。

### 阶段 1：C++ 基本功（第 1–3 周，持续）

**学**：RAII、所有权、`unique_ptr`/引用/值语义、`const`、移动语义、`string_view`、`filesystem`、`chrono`、`optional`、`expected`、`span`、头文件与 ODR、编译/链接错误怎么读。

**做**：`Item`、`Result`、`RetentionPolicy` + 单元测试。

**验收**：不写 `new/delete`；能解释一个对象在什么时候析构；能看懂模板报错的第一行。

### 阶段 2：设计原则（第 2–4 周）

**学**：内聚/耦合、依赖倒置、端口与适配器、组合根、深模块（deep module）、信息隐藏、YAGNI、rule of three。

**做**：四个端口 + 内存实现 + `ListItems` 用例 + `list` 命令。

**验收**：能画依赖图；能解释"为什么 `upsertMany` 不是 `beginTransaction`"；能说出一个你决定**不**抽象的地方。

### 阶段 3：数据与持久化（第 4–6 周）

**学**：关系模型、主键/索引、事务、prepared statement、迁移、WAL、SQL 注入、N+1、分页。

**做**：SQLite repository + 迁移 + 集成测试；`scan` 落库；`prune`。

**验收**：换掉内存实现，用例代码一行不改；能从空库迁移到最新版。

### 阶段 4：macOS 平台（第 5–8 周）

**学**：CoreFoundation 所有权（`CFRelease`）、`CFPreferences`、`getxattr`、dispatch source / FSEvents、ImageIO（`CGImageSource`）、TCC、`stat` 的 `st_birthtimespec`。

**做**：真实截图目录扫描 + 缩略图 + `watch`。

**验收**：在 `/tmp` 夹具上稳定；在真实桌面上能处理权限拒绝；跑一夜不崩、不泄漏。

### 阶段 5：并发与生命周期（第 7–9 周）

**学**：线程 vs 队列、数据竞争、TSAN、取消与优雅退出、背压、去抖。

**做**：`watch` 连续运行；Ctrl-C 干净退出；事件不丢不重。

**验收**：TSAN 无报告；`watch` 期间 `scan` 不冲突（或明确加锁/排队）。

### 阶段 6：质量与交付（第 8–10 周）

**学**：测试金字塔、假对象、golden test、clang-format/tidy、CI（GitHub Actions macOS）、日志分级、README/用法文档、语义化版本。

**做**：CI 跑构建+测试；`v0.1.0` tag；README 有截图/示例输出；`docs/adr/` 至少 3 篇。

**验收**：别人 clone 后照 README 能跑起来；CI 绿。

### 阶段 7：UI 与边界（第 10 周以后）

**学**：C ABI、不透明句柄、回调、Swift/C++ 互操作、SwiftUI 状态管理、AppKit 面板、拖拽、Quick Look。

**做**：C API + SwiftUI 历史列表（先只读）→ 角落缩略图 → 动作。

**验收**：UI 不 include 任何 SQL；CLI 仍然可用；同一份数据两个前端看到一致。

### 学习节奏（比书单重要）

- **一次只学一个概念，并在代码里用一次。** 这周学端口，就只改端口相关代码，不要同时上 SQLite。
- **写代码前写 10 行设计笔记**：问题、输入输出、依赖、不做什么。写完再动手。
- **每周五 30 分钟复盘**：这周哪个边界让你难受？是抽象错了，还是根本不需要抽象？记进 `docs/retro/`。
- **每月回读一篇自己写的 ADR**：如果理由已经不成立，改代码或改 ADR。
- **卡住时先缩范围**：删功能、加日志、写最小复现，而不是加抽象。

### 第一个月不要碰

- Swift / SwiftUI（等 CLI 稳定）；
- 多线程 / FSEvents 递归监听；
- 内容 hash、OCR、标签、搜索；
- 签名、公证、自动更新；
- 任何"框架"或"通用引擎"。

### 时间预期

- 到 v0.1（CLI 完整）：**8–12 周**，每周 8–12 小时；
- 到 UI MVP：再 **2–3 个月**；
- 这不是"学会 C++"的时长，只是"能独立做出一个结构正确的 CLI"的时长。

---

## 14. 里程碑与提交序列

| 里程碑 | 内容 | 新概念 | 完成定义 |
|---|---|---|---|
| M0 | 走通骨架 | CMake、Git、测试框架 | `--help`/`version`、一个测试、CI 可后补 |
| M1 | 领域模型 | RAII、`expected`、值对象 | `Item`/`RetentionPolicy` + 测试 |
| M2 | 内存版用例 | 端口、依赖注入、假实现 | `list` 用假数据跑通 |
| M3 | SQLite | schema、事务、迁移 | `scan` 落库、`list` 读库、集成测试 |
| M4 | 真实扫描 | CFPreferences、xattr、ImageIO | 真实桌面可扫、可出缩略图 |
| M5 | 监听 | dispatch source、去抖、取消 | `watch` 稳定、Ctrl-C 干净 |
| M6 | 策略与配置 | 保留策略、配置优先级、JSON | `prune` + `config` + `--json` |
| M7 | 交付 | 文档、CI、tag | `v0.1.0`，README 可复现 |

### 前 10 个 commit（建议）

1. `chore: cmake skeleton, help and version`
2. `feat(domain): Item, ItemId and Result`
3. `test(domain): retention policy boundaries`
4. `feat(core): repository port and in-memory implementation`
5. `feat(app): list items use case`
6. `feat(cli): list command with text output`
7. `feat(sqlite): schema v1 and repository`
8. `feat(app): scan library use case with fakes`
9. `feat(macos): screenshot folder source and ImageIO toolkit`
10. `feat: watch, prune, json output, docs; tag v0.1.0`

每个 commit 只做一件事，能单独构建、测试通过。**这是版本控制，也是架构纪律。**

---

## 15. 架构决策记录（ADR）

每做一个"以后不好改"的决定，写一页：

```text
# 0002 用 SQLite 做索引，文件系统是真源

## 背景
需要按时间列出历史截图，并支持以后搜索；截图可能被用户在 Finder 里删掉/移走。

## 选项
A. 只扫目录，不建库（每次全量）
B. JSON 文件存索引
C. SQLite 存索引

## 决定
C。文件系统是真源，数据库是索引；启动/扫描时对账。

## 后果
+ 启动快、可分页、以后加 FTS
- 多一个依赖和迁移负担
- 必须处理"DB 有、磁盘没有"
```

建议先写三篇：`0001-cli-first`、`0002-sqlite-as-index`、`0003-ports-only-where-needed`。

写 ADR 的好处：逼你把"直觉"变成"可被反驳的理由"。面试/复盘时，这比代码更能说明你会架构。

---

## 16. 反模式清单（看到就停）

1. **Clean Architecture 角色扮演**：20 个接口、0 个能跑的命令。
2. **`main.cpp` 一千行**：参数、SQL、业务、输出全在里面。
3. **全局单例数据库**：谁都能连，谁都能写，测试互相污染。
4. **domain 里出现 SQL/JSON/NSURL**：依赖方向已经反了。
5. **给每个类配一个 `IFoo`**：只有一个实现时，接口是负担。
6. **用异常做控制流**：文件不存在不是异常，是正常分支。
7. **忽略 `Result`**：`[[nodiscard]]` 加上，编译期提醒。
8. **假设路径永远不变**：文件会被移动、删除、重命名。
9. **过早多线程**：先正确，再并发。
10. **为 100 万张截图设计**：你现在只有 200 张。

---

## 17. 怎么判断一个边界该不该存在

四个问题，任何一个答"是"才值得抽象：

1. **它会变吗？**（SQLite 会换吗？截图目录规则会变吗？）
2. **我能独立测试它吗？**（能不能不启动 App 测保留策略？）
3. **换实现时，调用方要改吗？**（换存储要不要改用例？）
4. **依赖方向对吗？**（稳定的一侧定义接口，易变的一侧实现）

再加两条经验：

- **rule of three**：同样的东西出现第三次再抽象；
- **design twice**：动手前画两个方案，比较"哪里会疼"，再选。

---

## 18. 学习资源（少而精）

**架构与工程**

- 《A Philosophy of Software Design》（Ousterhout）——先读这本，短、直击模块边界；重点第 4–7、11 章。
- 《The Pragmatic Programmer》——工作流与习惯；第 1–2、7–8 章。
- 《Software Engineering at Google》（免费在线）——测试、代码评审、依赖管理；选第 4–5、9、15 章。
- 《Clean Architecture》——端口/适配器；批判性地读，别照搬。

**C++**

- 《A Tour of C++》（Stroustrup）——快速过语言全貌。
- 《Effective Modern C++》（Meyers）——`unique_ptr`、移动、lambda 捕获。
- cppreference.com——查标准库，别背。
- C++ Core Guidelines——当词典，不要一次读完。

**工具与平台**

- Pro Git（免费）第 2–3 章；
- CMake 官方 Tutorial 第 1–4 步；
- SQLite 官方文档：Quick Start、WAL、Transactions、`PRAGMA user_version`；
- Apple：FSEvents Programming Guide、ImageIO、CFPreferences、TCC 相关文档。

**读代码**

- 读 doctest / nlohmann/json 的某个头文件，看别人怎么组织接口；
- 读 SQLite 的 `shell.c` 里一个命令的实现（只看一个）；
- 读你自己三个月前的代码——最有效的架构课。

---

## 19. v0.1 验收清单

- [ ] `niceshot --help` 列出全部命令与退出码
- [ ] `scan` 幂等：跑两次，第二次 `added=0`
- [ ] `list --json` 输出稳定、可被 `jq` 解析
- [ ] `thumb` 对不存在的 id 返回 3，不崩溃
- [ ] `watch` 连续跑 1 小时不崩、不漏事件
- [ ] `prune` 默认不删任何东西；`--apply` 只删索引/缩略图
- [ ] 空库、坏库、无权限、文件被删——四种情况都有测试
- [ ] `niceshot_core` 不链接任何 Apple framework
- [ ] 核心用例全部能用假实现测
- [ ] README 能让别人从零构建
- [ ] `docs/adr/` 至少 3 篇
- [ ] tag `v0.1.0`

---

## 附：第一周每天做什么

| 天 | 任务 | 学到 |
|---|---|---|
| 1 | 确认 `cmake`（已装）；`git init`；CMake 最小工程；`--help`/`version` | 构建、Git、argv |
| 2 | 加 doctest、一个测试、CTest；README 骨架 | 测试、目标划分 |
| 3 | `Item`/`ItemId`/`Result` + 测试 | 值语义、`expected` |
| 4 | `RetentionPolicy` + 边界测试（0 天、31 天、未来时间） | 纯函数、假时钟 |
| 5 | 写 `ports/item_repository.h` + 内存实现；画依赖图 | 端口、依赖倒置 |
| 6 | `ListItems` 用例 + `list` 命令（假数据） | 用例、组合根 |
| 7 | 复盘：写 5 行 retro；提交；打 tag `v0.0.1` | 迭代节奏 |

第一周结束时，你应该能回答：**"为什么 `domain` 里没有 `#include <sqlite3.h>`？"** 如果答不出来，先别加新功能，回去把这条想清楚。