# 模组

`*.zip` 放进模组目录，启动时自动加载。

相关：[打包](packaging.md) · [数据包与压缩](data-packages.md)

## 目录

默认目录（越后面优先级越高）：

```
<exe>/mods  →  <cwd>/mods  →  用户数据目录 mods/（Godot 的 user://mods）
```

`--mods <dir>` 再追加（优先级更高），`--no-mods` 全部关掉。只认目录下的 `*.zip`，不进子目录。

用户数据目录在 Windows 上通常是
`%APPDATA%\Godot\app_userdata\Legacy Flame War\mods`。

## 两种 zip

| 类型 | 判定 | 行为 |
|---|---|---|
| 普通数据包 | 根目录没有 `index.json` / `index.json5` | 与本体合并（同名资源以模组为准），入口页「额外数据」会列出来 |
| 自定义游戏包 | 根目录有 `index.json` / `index.json5`，`paths` 指向包内 `prel.zip` / `data.zip` 等（至少 2 个） | 整包替换本体数据；多个时只认第一个（按文件名排序） |

自定义包的 `index.json` 两种写法都认：

```json
["prel.zip", "data.zip"]
```

```json5
{ type: "FULL", version: 0.1, title: "…", description: "…", author: "…", paths: ["prel.zip", "data.zip"] }
```

解析失败就降级按普通数据包处理。包内嵌套的 `*.zip` 同样支持任意压缩方法，见
[data-packages.md](data-packages.md)。

## 入口

| 入口 | 行为 |
|---|---|
| 启动扫描 | 依次加载默认目录与 `--mods` 里的 zip |
| 把 zip 拖进窗口 | 普通数据包热加载（同名去重，「额外数据」同步刷新）；自定义包自动重启换成它 |
| 入口页「附加模组」 | 弹系统文件对话框，选中的 zip 走同一条热加载路径 |
| 入口页「自定义游戏」 | 选 **full.zip**，暂存到 `user://custom-game/pending.zip` 后自动重启加载（只对本次会话生效） |

诊断参数：`--drop <zip>` 走拖拽那条信号链，`--custom-game <zip>` 走自定义游戏路径。

## 顺序与禁用

目录里可放 `mods.json5`（或 `mods.json`），改完重启生效：

```json5
{
  // 先列出的先加载，后面的覆盖前面的；没列出的按文件名排在最后
  order: ["基础模组.zip", "覆盖补丁.zip"],
  // 不加载
  disabled: ["先别用的模组.zip"],
}
```
