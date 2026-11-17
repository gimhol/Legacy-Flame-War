# TS → C++ 同步修复进度（2026-11-18）

配套审计：[sync-audit-2026-11.md](sync-audit-2026-11.md)（48 个漂移文件逐项结论）。
本文只记“已修”与“待修”，按轮次追加。

## 本轮已修（原生构建通过）

| 审计项 | 修复内容 | 文件 |
| --- | --- | --- |
| `_.ts` 联机 Logic 未注册 | 给 `NetworkLogic` 注册 `NetworkModalLogic` / `RoomLogic` / `RoomsLogic` 三个别名（数据里用的就是这三个名字） | `lfw/ui/component/component_registry.cpp` |
| `GameModeFSMState` 查不到 `score_board` | `find_child` → `search_node`（score_board 现在在 `__center/__design` 下） | `lfw/ui/component/game_mode_fsm_state.cpp` |
| `CMD_F4` 无弹窗分支 | 按 TS：有弹窗时只关最上层一个（`ignore_esc` 为真也 break，不下探） | `lfw/cmds/cmd_f4.cpp` |
| `xml_x_bg_layer` 丢字段 | 补 `width_percent`（含 `w_percent` 兜底）与 `center` 的读、写 | `lfw/dat_translator/xml/xml_x_bg_layer.cpp` |
| `WorldDataset` 缺字符串默认值 | `weapon_rain_oids` / `weapon_rain_groups` 默认 `''`，位置与 TS 字段顺序一致 | `lfw/world_dataset.cpp` |
| `Camera` 视口公式 | 横/纵可见尺寸改用 `dataset.screen_w/h`（`world.view_w/h`）：去掉 `/zoom`、`max(min_cam_x, cam_r - view_w)`、`height <= view_h`、`cam_max_y` 与加速度分母统一 | `lfw/camera.cpp` |
| `CameraCtrl` 跟随坐标 | `cam_x/cam_y` 去掉 `zoom` 除法，改用 `view_w/h` | `lfw/ui/component/camera_ctrl.cpp` |
| `SliderHandle` / `ScrollView` 场景→UI | 新增 `World::ui_from_scene` / `scene_from_ui`（`dataset.screen_w/h`），替换写死的 794/450 | `lfw/world.h`、`lfw/ui/component/slider_handle.cpp`、`lfw/ui/component/scroll_view.cpp` |
| `GamePrepareLogic` / `DemoModeLogic` / `DanmuGameLogic` 视宽 | 统一用 `world().view_w()`，删除 794 常量与 `zoom_x` 除法 | 三个组件 |
| `TiledImage` / `CoverImage` 未移植 | 新增组件：`TiledImage`（按 `design_w/h × repeat_x/y` 重算 `repeatX/Y`）、`CoverImage`（`cover_scale` 覆盖缩放），并注册 `TiledImage`/`TileImage`/`CoverImage` | `lfw/ui/component/tiled_image.*`、`cover_image.*`、`component_registry.cpp` |

验证：`node native/tools/native.mjs build` 通过；`lfw_run --root public --zip lf2s/origin/prel_data --frames 120` 跑完，`errors=0`、`warns=1`（既有）。

## 待修（按优先级）

1. **节点级 `alignment` 落地**：C++ `UINode` 无 `alignment` 字段与 `update()` 钩子；`cook_ui_info` 也不归一化。需要移植 `apply_alignment`（含百分比、offset、followed、once）。
2. **`touchable`**：cook/`CookedUIInfo`/`UINode` 全链路缺失，`touchable:false` 不生效（浮层会吃指针）。
3. **根页 `follow_screen` + 分辨率**：`RENDER_RESOLUTION_PRESETS`、`viewport_from_aspect`/`resolution_viewport`/`parse_aspect`，`World.settings`（`sync_render` 从 dataset 迁走），`MiscSettingsLogic` 的 `render_resolution_row` 与 `ArrowSwitcher::set_items`。
4. **`TextInput` placeholder**：props + Godot `LineEdit::set_placeholder`，以及 text 改 props-backed。
5. **`BgLayer` 渲染端 `width_percent`**：XML 读写已补，但 `BgLayerInfo` 访问器 + Godot 背景渲染还未消费（Web 端在 `BgLayerRender.ts`）。
6. **`World` 的 restrict/`drop_through`**：`Ground` 审计备注指出 TS 已按上一帧所在侧退回、`y` 不抬升；C++ 仍走 `ground.block()`。
7. 剩余 medium/low：`CMD_ENTITY` 数字格式化与 `--state` 校验、`WorldDataset` 移除 `sync_render`、`TextInput` props-based text、`UIInputHandle` 相机插值（当前等价）等，见审计报告。

## 工具注意

`native/tools/gen_defines_*.mjs` 会**整文件重写** `lfw/defines/*`，把 C++ 侧手工追加的辅助函数（`gk_label_table`、`difficulty_label`、`attcking_states` 等）删掉。同步 `fields_gen`/`runtime_gen` 前必须先解决「生成 + 保留手写块」的流程（例如把手写块移到独立头文件），否则会编译失败。本轮因此未提交生成器产物，`bg_layer`/`WorldDataset` 只做了定点手改。

## 第 2 轮（2026-11-18 深夜）

| 审计项 | 修复内容 | 文件 |
| --- | --- | --- |
| `UINode` 节点级 `alignment` 未落地（high） | `CookedUIInfo` 增加 `alignment`/`touchable` 访问器；cook 透传 `alignment` 并解析 `touchable`；`UINode` 构造读取两者，`update()` 里按 TS 语义应用四边对齐（left/right/top/bottom 的 `left/right/mid/百分比`、`offset`、`followed` 的 `parent:self:bro:id:name` 形式、`once`、双侧拉伸）；子节点循环补 `flush_flex` 顺序 | `lfw/data/ui_info.h/.cpp`、`lfw/ui/cook_ui_info.cpp`、`lfw/ui/uinode.h/.cpp` |
| `touchable` 未 cook（high） | 按 `local_only` 的模式解析并传给 `UINode`（默认继承父节点，根为 true） | 同上 |
| `TextInput` placeholder（medium） | 组件新增 `placeholder()` 读 `props.placeholder`；Godot 输入框补 `placeholder` 字段并在 `LineEdit::set_placeholder` 中同步（纳入 style 签名，避免每帧重设） | `lfw/ui/component/text_input.*`、`host/godot/lfw_godot_renderer.h`、`lfw_godot_ui_renderer.cpp`、`lfw_godot_text_input.cpp` |
| `CMD_ENTITY` 数值格式/校验（low） | `num()` 改为四舍五入 3 位、非有限值输出 `-`；`--state` 先用 `Number()` 校验 NaN 再查状态表（与 TS 文案一致） | `lfw/cmds/cmd_entity.cpp` |

验证：原生构建通过；`lfw_run --frames 120` 跑完 `errors=0`（warns 既有 1 条），世界摘要与改动前一致；两个 Godot 宿主文件用 `compile_commands` 里的命令做了语法检查通过；TS 全量 jest 212 用例通过（本侧未改 TS）。

## 第 3 轮

| 审计项 | 修复内容 | 文件 |
| --- | --- | --- |
| 根页 `follow_screen`（high） | cook 根分支：尺寸取 `dataset.screen_w/h`（为 0 回退 `SAFE_SCREEN_WIDTH/HEIGHT`），按 `flex` 是否 `display:flex && (fit_w || fit_h)` 决定 `follow_screen`，为真时 `pos=[0,0,z]`；`CookedUIInfo`/`UINode` 增加 `follow_screen`；`UILayer::create_page` 按逻辑视口 `resize+move_to`；新增 `UILayers::resize_follow_screen()`，并订阅 `world.on_dataset_change` 的 `screen_w/screen_h` 触发 | `lfw/ui/cook_ui_info.cpp`、`lfw/data/ui_info.h/.cpp`、`lfw/ui/uinode.h/.cpp`、`lfw/ui/uilayer.h/.cpp`、`lfw/lfw.cpp` |
| `World` 的 restrict/`drop_through`（medium/high） | 被地形挡住时改为「按上一帧所在侧退回并 clamp，y 不再抬到地形高度」；`drop_through` 改为 TS 语义：`is_fighter && is_on_ground && terrain.through && (D 或 Defend) && y > ground_y - 1`，命中时 `y = ground_y - 1` | `lfw/world.cpp` |
| `schemas_gen.h` 过期 | 重跑 `gen_defines_schemas.mjs`：补上 `ITerrainInfo` 的 `one_way/through/cliff/cliff_frame/cliff_respawn_y`，以及 `IWorldDataset` 的 `jump_hold_ctrl/cam_y_follow/weapon_rain_*/air_ctrl*` 等字段；运行期不再报 `unexpected key`（warns 1→0） | `lfw/defines/schemas_gen.h` |

验证：原生构建通过；`lfw_run --frames 300` 跑完 `warns=0 errors=0`（地形 schema 警告消失）；TS 全量 jest 212 用例通过（本侧未改 TS）。

## 第 4 轮

| 审计项 | 修复内容 | 文件 |
| --- | --- | --- |
| 分辨率设置链路（high） | 新增手写头 `render_resolution.h`：`viewport_from_aspect`（安全区只外扩）、`parse_aspect`、`RENDER_RESOLUTION_PRESETS`（13 档，含 4:3/16:10/21:9）、`DEFAULT_RENDER_RESOLUTION`、`render_resolution_index`、`resolution_viewport` | `lfw/defines/render_resolution.h` |
| `World.settings` 与 `sync_render` 迁移 | `WorldSettings{sync_render, render_w, render_h}`；`World::apply_render_resolution` 写 settings、按比例重算 `dataset.screen_w/h`（触发 follow_screen 重排）、通知渲染器；`World::set_sync_render` 变更时重启渲染并抛 `on_settings_change`；`IWorldRenderer` 增加默认空实现的 `set_render_resolution`；`WorldDataset` 移除 `sync_render` 默认值与访问器（与 TS 一致）；thin 宿主 `--sync-render`、Godot 帧调度改用 settings | `lfw/world.h/.cpp`、`lfw/world_dataset.h/.cpp`、`host/thin/lfw_thin.cpp`、`host/godot/lfw_godot_frame.cpp` |
| 设置页分辨率行 | `ArrowSwitcher::set_items`；`MiscSettingsLogic` 增加 `render_resolution()`、`sync_render_resolution()`，启动时填充预设标签、值变化调用 `world.apply_render_resolution`，恢复时同步当前档位；渲染帧率行改走 `world.set_sync_render`，世界回调改听 `on_settings_change`（sync_render / render_w / render_h） | `lfw/ui/component/arrow_switcher.*`、`lfw/ui/component/misc_settings_logic.*` |

验证：原生构建通过；`lfw_run --frames 60` `warns=0 errors=0`；`lfw_thin --sync-render 4` 正确切到 FPS=120；Godot 帧调度文件语法检查通过；TS 全量 jest 212 用例通过。

### 最后仍未同步（低优先级）

- `BgLayer` 渲染端的 `width_percent`（XML 读写已补，Godot 背景绘制未消费）。
- `TextInput` 的 `text` 改为 props-backed（C++ 仍以成员 `_text` 为准，行为等价）。
- `CMD_ENTITY` 少量取值强转边角。
- `native/tools/gen_defines_*.mjs` 会抹掉手工追加的辅助块，生成器流程需要整理。
- 审计里 169 个「无同名 C++ 文件」的 TS 文件（多为接口/再导出/UI 测试用例/Web 宿主）仍需逐个定性。

## 续（轮次上限恢复后）

| 项 | 内容 | 文件 |
| --- | --- | --- |
| `BgLayer` 渲染端 `width_percent` | `BgLayerInfo` 增加 `width_percent()/center()` 访问器；Godot 背景渲染按 TS 语义消费：缺省时 `absolute + 无图 + 有颜色` 按 100 处理；`absolute` 层按 `view_w / zoom_x * pct/100` 定宽，非 absolute 层用百分比宽度参与视差与 UV 循环宽度 | `lfw/data/bg_info.h/.cpp`、`host/godot/lfw_godot_renderer.cpp` |
| 169 个「无同名 C++ 文件」的 TS 模块定性 | 新增自动分类（type-only / re-export / ui-test / web-host / ported-elsewhere / gap）+ 人工确认结论，实际未移植只剩 `fit_text`（联机行标签截断，low）与少量未用谓词 | `docs/sync-unported-2026-11.md`、`build/triage_unported.mjs` |

验证：原生构建通过；`lfw_run --frames 120` `warns=0 errors=0`；Godot 背景渲染文件语法检查通过。
