# 远端数据包（HTTP + 落盘缓存）

Godot 宿主可以像 Web 版那样，直接从远端拉数据包并缓存到磁盘。相关：[数据包读取](data-packages.md) ·
[模组](mods.md) · [命令参考](commands.md)

## 用法

```bash
# 远端包：给 info json 的地址（不是 zip 本身）
node tools/native.mjs godot-run -- \
  --zip https://example.com/packs/prel.zip.json \
  --zip https://example.com/packs/data.zip.json \
  --version-check https://example.com/latest.json
```

本地包和远端包可以混用，按给定顺序加载；本地在前，远端在后（`--zip` 顺序即加载顺序）。

## info json

和 Web 版同一份格式（`__info.json` / `*.zip.json`），至少要有 `url` 与 `md5`：

```json
{
  "type": "data",
  "url": "data.zip",
  "md5": "d4394327a017a7b9544194d2256e048d",
  "version": "1.0.0",
  "title": "data"
}
```

`url` 可以是相对 info json 的路径（`data.zip` → 同级目录），也可以是绝对 `http(s)` 地址。
核心会把 `?md5=<md5>` 追加到最终下载地址上（内容变 ⇒ URL 变，CDN 边缘缓存必然回源；
内容不变 ⇒ URL 稳定，本地缓存能复用）。

## 流程

```
__info.json（HTTP）
      │  md5 = 内容标识
      ▼
user://cache/data/cache.json  ── 命中 ──▶ <md5>.zip（惰性读，整包不进内存）
      │  未命中
      ▼
Range 断点续传 ──▶ <md5>.zip.part ──▶ md5 校验 ──▶ 改名 <md5>.zip + 写索引
                                        └─ 不符 ──▶ 清掉重下一次
```

实现在宿主这一侧（`host/godot/lfw_godot_{http,remote_cache}.cpp` 覆写 `ILfwHost` 的
`import_as_*` / `zip_download` / `zip_cache_*` / `zip_read_blob`），核心的
`LFW::load_zip_from_url` 一行没改 —— Web 版走的是同一条链。

## 缓存位置

```
<Godot user://>/cache/data/
  cache.json         索引：md5 → { file, size, version, time }
  <md5>.zip          落盘的包（下次启动直接命中，不再联网）
  <md5>.zip.part     下载中的半截（下次启动按 Range 续传）
  version.json       latest.json 的副本 + 抓取时间（TTL 6 小时）
```

Windows 上 `user://` ≈ `%APPDATA%\Godot\app_userdata\Legacy Flame War\`。删掉整个
`cache/data/` 就等于清空缓存。

## 日志

远端相关行都走 `log` 通道，`--console-echo` 时能在 stdout 看到：

```
[remote] cache C:/…/cache/data (2 packs)
[remote] cache hit: d55fde8ddabae1c5f2f59f682658f1d8.zip
[remote] download: http://127.0.0.1:8899/data.zip?md5=d439…
[remote] 0% 65536/34045173 http://127.0.0.1:8899/data.zip?md5=d439…
…
[remote] cached: d4394327a017a7b9544194d2256e048d.zip (34045173 bytes)
[remote] version check: local=v0.1.64 latest=v9.9.99 (update available)
```

## 注意

- HTTP 是**同步阻塞**的（`ILfwHost` 的取数接口是同步的，Godot 的 `HTTPClient` 只能在加载期
  自旋 `poll`）。只发生在启动 / 进关的加载阶段，别指望在 `_process` 里跑。
- 下载进度会转成 `LFW::on_loading_file`（0~100%），加载页左下角那行文字直接显示。
- 校验失败**不会**污染缓存：`.part` 会被删掉，整包重下一次；仍然失败才报错退出加载。
- 服务端不支持 `Range`（回 200 而不是 206）时会自动清空重下，不会拼出坏包。
- 本地包优先：**同一份数据能放本地就别放远端**，远端适合做增量更新 / 首启下载。
- `--no-remote-cache` 仍然联网，只是把缓存放到 `%TEMP%/lfw-remote-cache/<时间戳>/`，进程退出
  就整目录删掉（不碰 `user://`；验收 / 排障用）。
- 入口页「开始游戏」会再 `load` 一遍 `ZIPS()`，已经加载过的包（含远端 url）会直接跳过，
  不会重复下载 —— 见 `LFW::_loaded_zip_keys`。
