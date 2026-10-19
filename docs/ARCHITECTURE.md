# niceshot：学习脚手架与实现路线

这不是一份“照着抄”的教程。这里只有边界、契约、验收标准和踩坑提示；具体代码由你写。

## 0. 先换一种工作方式

看别人写代码，能看懂，但很难变成自己的能力。有效的循环是：

1. 你实现一个小目标。
2. 运行它，观察真实输出。
3. 卡住时，带着“我做了什么、看到什么、期望什么”来问。
4. 我 review 你的代码和设计，指出问题；除非你明确说“给我代码”，否则我不替你写。

每个里程碑都必须能运行、能演示。不要一次把整套架构建完。

## 1. v0.1 只做一条垂直切片

目标只有两个命令：

```text
niceshot scan     列出系统截图目录里已有的截图
niceshot watch    新截图出现时打印一行
```

暂时不做：数据库、缩略图、菜单栏 UI、保留策略、云同步。

`scan` 让你先看到真实数据；`watch` 让你看到“新事件”这条链路。两者跑通之后，再谈持久化和界面。

## 2. 目标架构

```text
                 ┌──────────────┐
                 │     CLI      │  参数解析、组装、输出
                 └──────┬───────┘
                        │
                 ┌──────▼───────┐
                 │     app      │  用例编排：scan / watch
                 └──────┬───────┘
                        │ 依赖接口
                 ┌──────▼───────┐
                 │    ports     │  内核需要的抽象
                 └──────▲───────┘
                        │ 实现
                 ┌──────┴───────┐
                 │  adapters    │  macOS：xattr、FSEvents、偏好
                 └──────────────┘

        domain：纯值类型和规则，被以上各层使用，不依赖它们
```

依赖规则只有一条：**macOS 头文件只能出现在 `adapters/`。**

- `domain/` 不 include CoreFoundation、FSEvents、dispatch。
- `app/` 不知道 xattr 和 FSEvents，只知道“扫描截图”“监听新文件”这两个意图。
- `ports/` 是 app 拥有的接口，adapter 来实现；不要让 adapter 定义接口再让 app 依赖 adapter。
- `cli/` 是组合根：创建具体 adapter，注入 app，处理输出和退出码。

### 但现在不要把所有层都建出来

第一刀允许 `cli` 直接调用 adapter 的具体函数。先让 `scan` 跑通，再在出现第二个实现时抽接口。

抽象应该由“变化”驱动，而不是由“架构图”驱动。你会在 M3 同时拥有 polling 和 FSEvents 两个 watcher，那时抽 `ScreenshotWatcher` 接口才有真实意义。

## 3. 建议目录

```text
src/
  domain/
    screenshot.h/.cpp        # Screenshot 值类型（M1）
  ports/
    screenshot_source.h      # 扫描接口（M1 可选，M3 必须）
    screenshot_watcher.h     # 监听接口（M3）
  app/
    scan.cpp                 # 用例：扫描并输出（M1 可先不做）
    watch.cpp                # 用例：baseline + 新文件策略（M2）
  adapters/
    macos/
      screenshots.cpp        # 截图目录、xattr、scan（M1）
      polling_watcher.cpp    # 轮询实现（M2）
      fsevents_watcher.cpp   # FSEvents 实现（M3）
  cli/
    main.cpp                 # 参数解析、组装
tests/
```

文件名可以不同，但**边界**要守住：`domain` 纯 C++，`adapters` 才碰 macOS。

## 4. 数据契约

先定义唯一的核心值类型：

```cpp
struct Screenshot {
    std::filesystem::path path;
    std::chrono::sys_seconds captured_at{};
    std::uintmax_t size_bytes = 0;
};
```

设计问题，先自己想再写：

- `captured_at` 用 birthtime 还是 mtime？birthtime 为 0 时怎么办？
- 时间用 `system_clock` 还是 `file_clock`？跨重启、跨时区时哪个更稳？
- 这个类型需要方法吗？还是保持纯数据，把行为放在 app？

M1 只需要这三个字段；不要提前加 `id`、`hash`、`thumbnail`。

## 5. 里程碑

### M1：`scan`，先看到真实截图

**实现**

- `screenshot_folder()`：读 `com.apple.screencapture` 的 `location-screenshot`，再读 `location`，最后回退 `~/Desktop`。
- `is_screenshot(path)`：扩展名是图片，并且存在 `com.apple.metadata:kMDItemIsScreenCapture` 这个 xattr。
- `scan_screenshots(folder)`：遍历目录，过滤、`stat`、按拍摄时间倒序。
- `cli` 增加 `scan` 子命令。

**验收**

```bash
./build/niceshot scan
```

- 输出截图目录和截图数量。
- 只列出带截图 xattr 的图片。
- 普通 `.png`（没有 xattr）不出现。
- 目录不存在时给出错误，退出码非 0。
- 同样的输入，连续运行输出稳定。

**提示**

- 偏好 API：`CFPreferencesCopyAppValue`，头文件 `CoreFoundation/CFPreferences.h`。
- xattr：`getxattr(2)`，头文件 `sys/xattr.h`；`getxattr(..., nullptr, 0, ...)` 只探测存在性。
- `mdls` 走 Spotlight，可能滞后；不要用它做实时判断。
- 用 `xattr -l <file>` 先确认样本文件真的有这个属性。
- `std::filesystem::directory_iterator` 要处理权限错误；`skip_permission_denied` 可以先让你跑起来。

**先写测试**

至少覆盖：

- 图片扩展名 + 有 xattr → true
- 图片扩展名 + 无 xattr → false
- 非图片扩展名 + 有 xattr → false
- 目录不存在 → 空结果/错误

xattr 测试可以在临时目录里用 `setxattr` 造样本，不必依赖真实截图。

### M2：`watch`，先用轮询把策略跑通

不要一上来写 FSEvents。先写一个 500ms 的轮询 watcher：

- 启动时扫描一次，把路径放进 `known`，作为 baseline。
- 每次扫描，发现 `known` 里没有的截图就回调，然后加入 `known`。
- `Ctrl+C` 能干净退出。

**验收**

```bash
./build/niceshot watch
```

- 启动时不打印已有截图。
- 复制一张带 xattr 的截图进目录，打印一行 `new ...`。
- 创建一个普通 `.png`，不打印。
- 退出后进程不残留。

**为什么先轮询**

它把“什么算新截图”这个**策略**和“怎么收到文件事件”这个**机制**分开了。策略正确后，M3 只替换机制，行为不变。

M2 的 watcher 可以先用一个简单接口：

```cpp
class ScreenshotWatcher {
public:
    virtual ~ScreenshotWatcher() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};
```

回调怎么传、是否用 `std::function`、错误怎么报，由你决定。先写最简单的版本。

### M3：FSEvents，替换轮询

现在把 polling 和 FSEvents 放到同一个接口后面，让 CLI 不关心用的是哪个。

**FSEvents 生命周期**

```text
FSEventStreamCreate
  → FSEventStreamSetDispatchQueue
  → FSEventStreamStart
  → callback(paths, flags)
  → FSEventStreamStop
  → FSEventStreamInvalidate
  → FSEventStreamRelease
```

**必须理解的参数**

- `sinceWhen = kFSEventStreamEventIdSinceNow`：不要回放历史事件。
- `latency`：0.1–0.3 秒足够；它是合并窗口，不是精确延迟。
- `kFSEventStreamCreateFlagFileEvents`：要文件级事件；否则只有目录级。
- `kFSEventStreamEventFlagItemCreated/Modified/Renamed`：你关心的三类。
- `kFSEventStreamEventFlagMustScanSubDirs`：需要重新扫描目录。

**验收**

- 与 M2 行为完全一致。
- 空闲时不再每 500ms 扫目录。
- 用 `top -pid $(pgrep niceshot)` 或活动监视器对比 M2/M3 的空闲 CPU。
- `Ctrl+C`、`SIGTERM` 都能退出。

**你会踩的坑**

- 回调的 `info` 必须指向你的 watcher 实例；`FSEventStreamContext` 要正确初始化。
- 如果用 dispatch timer，`dispatch_source_set_event_handler_f` 不会自动传 `this`，要配合 `dispatch_set_context`。
- 后台启动的进程可能继承 `SIGINT = SIG_IGN`；只 block + `sigwait` 会一直等不到。先恢复默认处置，再 block。
- 事件到达时 xattr 可能还没写完。先记录现象，不要急着加复杂重试；如果真实截图稳定漏，再加 pending 重试。
- 目录被重命名/移动时，stream 可能失效；先处理“目录变化就重建 stream”。

### M4：复制旧截图不算新截图

M2/M3 只按路径判断，所以复制一张旧截图会被当成新文件。改成内容身份：

- 启动时对 baseline 里每张截图算 SHA-256，存进 `known_hashes`。
- 新候选文件先算哈希；哈希已存在就只加入 `known`，不回调。
- 哈希不存在才回调，并加入 `known_hashes`。

**验收**

- 复制一张已有截图到同目录：不打印。
- 复制后追加一个字节（内容变了）：打印。
- 重启进程后行为一致（如果先做内存版，重启会重新 baseline，这是可以接受的中间状态）。

**设计问题**

- 哈希什么时候算？文件可能还没写完。
- 两张真正相同的实时截图要不要都保留？
- 哈希存内存还是 SQLite？先内存，M6 再持久化。
- 大文件哈希的 CPU 成本可以接受吗？

### M5：自动跟随截图目录

FSEvents 只监听目录，不监听 `com.apple.screencapture`。所以还需要：

- 每 2 秒读一次偏好。
- 目录变了：停掉旧 stream，重建新 stream，重新做 baseline。
- 新目录里已有的截图不能报成“新”。

**验收**

- 用 `defaults` 临时改截图位置，`watch` 打印 `Watching <new folder>`。
- 新目录里已有的截图不打印。
- 改回原位置，仍然正常工作。
- 测试前备份偏好，测试后恢复；不要把这个测试留在系统里。

### M6：之后再做

- SQLite 持久化（截图、哈希、保留策略）。
- 缩略图（ImageIO）。
- 菜单栏 UI。
- 保留/清理策略。

在 M1–M5 没稳定前，不要碰这些。

## 6. 需要你写下来的设计决策

在仓库根目录建一个 `NOTES.md`，每个里程碑写三句话：

- 我做了什么选择？
- 为什么不选另一个？
- 还有什么不确定？

建议先回答：

1. “新截图”是事件属性还是状态差异？baseline 应该放在哪一层？
2. 截图的身份是路径还是内容？各自失败场景是什么？
3. 错误怎么传？`bool`、`std::optional`、`std::error_code`、`std::expected`，现在需要哪种？
4. 回调在哪个线程执行？如果以后有 UI，谁来切回主线程？
5. 哪些逻辑可以纯函数测试，哪些必须集成测试？

## 7. API 地图

| 主题 | 位置 |
|---|---|
| 截图目录偏好 | `CoreFoundation/CFPreferences.h`；`man 1 defaults` |
| xattr | `sys/xattr.h`；`man 2 getxattr`；`man 1 xattr` |
| FSEvents | `CoreServices.framework/.../FSEvents.framework/Headers/FSEvents.h` |
| FSEvents 指南 | Apple 的 *File System Events Programming Guide*（归档） |
| dispatch | `dispatch/dispatch.h`；`man 3 dispatch` |
| 信号 | `sigwait(2)` / `pthread_sigmask(3)` |

FSEvents 头文件里重点看：

- 生命周期总览
- `FSEventStreamCreateFlags`
- 事件 flags
- `FSEventStreamCreate` 的参数
- `FSEventStreamSetDispatchQueue` / `Start` / `Stop` / `Invalidate` / `Release`

## 8. 调试阶梯

按顺序排查，不要跳步：

1. `xattr -l <file>`：样本到底有没有截图属性？
2. `scan`：目录、扩展名、xattr、排序，哪一步不对？
3. `watch` 轮询版：baseline 和新文件判断对不对？
4. FSEvents 版：先打印原始事件路径和 flags，再做过滤。
5. 性能：空闲 CPU、事件合并、是否有重复回调。

常见误区：

- 用 `mdls` 判断新文件：Spotlight 可能还没索引。
- 用 `cp` 的 birthtime 判断新旧：`cp -p`/`ditto` 会保留旧时间。
- 把 FSEvents 当成“偏好变化通知”：它只监听文件系统路径。
- 在回调里做重活：回调线程被占住，事件会堆积。

## 9. 工作协议

- 你写实现；我 review 设计、边界、测试和命名。
- 提问时给三样东西：**目标、实际输出、期望输出**。
- 需要我直接给代码时，明确说“这次给我代码”；否则我只给提示和评审。
- 每个里程碑结束提交一次，提交信息写清楚“做了什么、怎么验证”。
- 卡住超过 30 分钟就先降级：回到上一个能运行的版本，缩小问题。

## 10. 第一个作业

只做 M1，不碰 FSEvents。

1. 建 `Screenshot` 值类型。
2. 实现 `screenshot_folder()`。
3. 实现 `is_screenshot()`。
4. 实现 `scan_screenshots()`。
5. 在 CLI 加 `scan`。
6. 写 3–4 个测试。
7. 用真实截图目录跑一遍。

做到这里就停，把输出和你的问题发给我。下一步再决定 M2 的 watcher 接口怎么定。