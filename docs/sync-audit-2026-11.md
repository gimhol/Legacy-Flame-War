# TS → C++ 同步审计（2026-10-01 ~ 2026-11-18）

## 范围与方法

- 变更来源：本仓库 `git log --since=2026-10-01 -- src/LFW src/DittoImpl`，共 294 个 TS 文件有改动。
- C++ 侧：`native/`（独立 git 仓库）。按目录/文件名映射后，48 个 TS 文件被判定为可疑漂移（TS 最后改动晚于对应 C++ 文件 2 小时以上），逐个核对。
- 口径：只比较运行时行为语义（字段、默认值、常量、公式、边界、生命周期、回调顺序）。C++ 命名/结构不同不算缺失；纯 TS 重构、类型声明、React/DOM/Web 专属不计。
- 生成物：本文件 + `sync-audit-2026-11.json`（机读）。

## 统计

- verdict：missing=5，not-applicable=4，partial=22，synced=17
- findings：high=19，medium=33，low=27

## high

| TS 文件 | 结论 | 问题 | C++ 证据 | 修复建议 |
| --- | --- | --- | --- | --- |
| src/LFW/ui/UINode.ts | partial | TS 新增节点级 alignment（字段 + set_alignment + 每帧 apply_alignment/once 一次性），C++ UINode 完全没有该字段与更新钩子，cook 出的 alignment 值无消费方，UI XML 里 alignment= 的节点不会被布局。 | native/lfw/ui/uinode.h:51-367、native/lfw/ui/uinode.cpp:669-698（update 无 alignment）；native/lfw/ui/xml_to_ui_info.cpp:792 只写入 alignment，native 无任何读取点 | 在 UINode 增加 alignment 字段与 set_alignment，并在 update() 开头按 alignment.ts 的 apply_alignment 语义对自身执行一次（返回 true 且 once 时置 _alignment_done）。 |
| src/LFW/ui/cook_ui_info.ts | partial | 窗口新增的 touchable 字段完全未移植：TS 在 cook 时解析 touchable 并经 UINodeBuilder 写入节点，用于指针命中判定；C++ 的 cook_ui_info、CookedUIInfo、UINode 构造都没有 touchable，native 全仓搜不到 u"touchable"，UI 数据里写 touchable:false 不生效，浮层仍会吃掉指针。 | native/lfw/ui/cook_ui_info.cpp:318-350（初始化/赋值块无 touchable）；native/lfw/data/ui_info.h:121-130（无 accessor）；native/lfw/ui/uinode.cpp:75-86（构造不读 touchable）；native 内无 u"touchable" 匹配 | 在 cook_ui_info.cpp:343 附近按 local_only 的模式解析 touchable，CookedUIInfo 增设 accessor，UINode 构造时 set_touchable(_info.touchable())，未设时沿用父节点。 |
| src/LFW/ui/cook_ui_info.ts | partial | 根页 follow_screen 逻辑缺失：TS 根节点尺寸取 view_w/view_h（缺省 794/450），仅当非 flex 自适应时 follow_screen=true 并把 pos 重置为 (0,0,z)；C++ 根分支无条件用 dataset screen_w/h 撑满且不重置 pos，也没有任何 follow_screen 消费方，分辨率变化时根页不会重新适配。 | native/lfw/ui/cook_ui_info.cpp:674-681（仅 dataset.get(screen_w/h)，无 follow_screen、无 pos 重置）；native 内无 follow_screen 匹配；screen_w 只在 cook 时读取 | 根分支按 ret.flex 的 display/fit_w/fit_h 计算 follow_screen，尺寸用 view 尺寸（dataset 缺省 794/450），follow_screen 为真时 pos=[0,0,pos.z]，并在 UILayer 提供屏幕变化时按 follow_screen 重排。 |
| src/LFW/Camera.ts | partial | TS 纵向可见高度改用 world.view_h（=dataset.screen_h）决定 height<=view_h 提前退出与 cam_max_y；C++ 仍用常量 Defines.MODERN_SCREEN_HEIGHT，屏幕高与环境不等于该常量时，纵向退出条件与可移动上限均与 TS 不符。 | native/lfw/camera.cpp:104（height <= modern_screen_height()）、115（view_h = modern_screen_height()/zoom_y） | 把 view_h 改为 dataset.screen_h()，并让 camera.cpp:104 与 115 统一使用该 view_h。 |
| src/LFW/cmds/CMD_F4.ts | partial | 弹窗分支完全缺失。TS 在遍历每层时先看 layer.modal：有弹窗就关弹窗并 break，不再下探；C++ 完全没有 stock modal() 判断，直接走 pages 长度逻辑。 | native/lfw/cmds/cmd_f4.cpp:26-32（层循环内无 modal 判断） | 在取 pages 长度前加：if (auto* m = layer->modal()) { if (!m->ignore_esc()) layer->close_modal(ui::UICloseModalOpts{}); break; } |
| src/LFW/cmds/CMD_F4.ts | partial | 有弹窗时 C++ 的行为错两层：TS 只关最上层一个弹窗（on_modal_close 只收该弹窗）；C++ 会调 layer->pop()，而 pop() 先 close_modals() 关掉全部弹窗再弹页面，回调次数与页栈结果都不同。 | native/lfw/cmds/cmd_f4.cpp:31 → native/lfw/ui/uilayer.cpp:168（pop 开头 close_modals()） | 弹窗路径必须单独走 UILayer::close_modal(单弹窗)，不要复用 pop()；同时补差分用例覆盖 open_modal 后 F4。 |
| src/LFW/ui/component/_.ts | partial | TiledImage 未移植：C++ 既无该类也未注册，cls=TiledImage 的公共背景实例化失败，背景不再按设计尺寸重算平铺，直接退成 1x1。 | native/lfw/ui/component/component_registry.cpp:101-189 无 TiledImage 条目；native 下 grep TiledImage/tiled 零命中，无等价实现；数据 public/builtin_data/launch/bg.ui.json:13 用该 cls（design 794x450, repeat 20x11）；未注册时 native/lfw/factory.cpp:270 仅 warn 后 continue | 新增 TiledImage 组件（repeat = node.w/design_w*repeat_x、node.h/design_h*repeat_y）并注册 "TiledImage"/"TileImage" 两个名字。 |
| src/LFW/ui/component/_.ts | partial | CoverImage 未移植：C++ 无 cover_scale，加载页的 CoverImage(target=root) 未注册被跳过，等待图不再按 max(sw/dw, sh/dh) 缩放覆盖。 | component_registry.cpp:101-189 无 CoverImage；native 全仓无 cover_scale 及 design_w/design_h 处理；数据 lf2s/origin/prel_data/ui/loading_page.ui.json5:26、demo_loading_page.ui.json5:29、network_loading_page.ui.json5:26 均用 cls=CoverImage | 实现 CoverImage（scale=max(target.w/design_w, target.h/design_h)，target=="root" 取 root）并在 registry 注册 "CoverImage"。 |
| src/LFW/ui/component/_.ts | partial | 联机三个 Logic 未按数据实际类名注册：C++ 只注册了 NetworkLogic，而数据写 NetworkModalLogic/RoomLogic/RoomsLogic，均被 warn 跳过，合并后的网络大厅逻辑无任何节点能拉起。 | component_registry.cpp:182 仅 regist_one<NetworkLogic>(u"NetworkLogic")，NetworkLogic::TAGS 也只有该名（network_logic.cpp:1427）；数据 network_modal.ui.xml:2、network_room_page.ui.xml:2、network_rooms_page.ui.xml:2 用的是另三个名字；全仓库无 cls="NetworkLogic" 引用，故 NetworkLogic 永不会被创建 | 在 component_registry.cpp 追加 regist_one<NetworkLogic>(u"NetworkModalLogic")、u"RoomLogic"、u"RoomsLogic" 三个别名（或把三名并入 NetworkLogic::TAGS）。 |
| src/LFW/ui/component/MiscSettingsLogic.ts | missing | 窗口新增的 render_resolution 行控件（ArrowSwitcher, row "render_resolution_row"）在 C++ 完全缺席，无 getter、无缓存字段。 | native/lfw/ui/component/misc_settings_logic.h:27-36 仅 10 个 getter；misc_settings_logic.cpp:94-104 DPtr 无对应成员，grep native/lfw 无 render_resolution 符号 | 在 h 中声明 ArrowSwitcher* render_resolution()，DPtr 增加 ArrowSwitcher* k，用 row_switcher(u"render_resolution_row") 缓存。 |
| src/LFW/ui/component/MiscSettingsLogic.ts | missing | 分辨率行初始化与值改变回调未移植：set_items 填充预设标签，值变化写 render_w/h 并应用分辨率；C++ on_start 无此段，且 C++ ArrowSwitcher 没有 set_items。 | native/lfw/ui/component/misc_settings_logic.cpp:226-345 on_start 结束无该逻辑；arrow_switcher.h:32-46 无 set_items（仅 slider_handle.h:44 有） | 为 ArrowSwitcher 增加 set_items，并在 on_start 注册 render_resolution 的 on_value_changed（含 null 回退到预设 0）。 |
| src/LFW/ui/component/MiscSettingsLogic.ts | missing | World.apply_render_resolution 与 RENDER_RESOLUTION_PRESETS / render_resolution_index / resolution_viewport 未移植，native 无 render_w/render_h 字段。 | native/lfw 全库 grep 无 RENDER_RESOLUTION/render_w/render_h（仅 render_worker_id）；native/lfw/world.h:88-90 无 settings 成员 | 在 native defines 增加预设表与索引函数，World 增加 apply_render_resolution(w,h) 写 render_w/h 并按宽高比映射 screen_w/h。 |
| src/LFW/dat_translator/xml/xml_x_bg_layer.ts | partial | xml_2_bg_layer 未解析 width_percent：TS 于 81a84a99a 新增该字段，并支持 w_percent 兜底（null 链），C++ 在 dh 之后直接跳到 uv_loop，读入 XML 时百分比宽度被丢弃。 | native/lfw/dat_translator/xml/xml_x_bg_layer.cpp:44-45 | 在 dh 之后补 width_percent 解析，回退链 width_percent → w_percent → 原值，与 TS 行序一致。 |
| src/LFW/dat_translator/xml/xml_x_bg_layer.ts | partial | xml_x_bg_layer 未写回 width_percent 属性：TS 在 dh 之后 set_attr，C++ 写出的 XML 丢失该字段，导致 C++ 端读取/回写往返后数据不一致。 | native/lfw/dat_translator/xml/xml_x_bg_layer.cpp:74-75 | 在 set_attr(dh) 之后补 layer->set_attr(u"width_percent", field_or(l, u"width_percent"))。 |
| src/LFW/defines/defines.ts | missing | RENDER_RESOLUTION_PRESETS 整表缺失：窗口内新增 1024×768、1600×1200、1280×800、1920×1200、2560×1080、3440×1440 六条，且所有固定尺寸项 label 改为 `W×H (a:b)` 并新增 aspect 字段。native 全仓无此标识，C++ 定义表也未收录。 | native/lfw/defines/runtime_gen.cpp:104-169（kRaw 无该条目） | 重跑 native/tools/gen_defines_runtime.mjs 收录预设，并补上设置页与 world 侧消费。 |
| src/LFW/defines/defines.ts | missing | viewport_from_aspect / resolution_viewport / parse_aspect 三个新函数缺失：逻辑视口按比例只向外扩、保证安全区完整可见（16:9→800×450、4:3→794×596、21:9→1050×450），且 w/h 为 0 或负时返回无效。C++ 逻辑视口恒为 794×450，无 aspect 概念，宿主也无 --aspect 等价参数。 | native/lfw/world_dataset.cpp:85-86; native/lfw/world.cpp:926-927 | 在 defines 模块实现这三个函数，并给 world 增加按比例改写 screen_w/screen_h 的入口。 |
| src/LFW/ui/component/CameraCtrl.ts | missing | 自动跟随镜头的目标坐标公式未同步。窗口内把 cam_x = staring.x - screen_w/(2*zoom_x)、cam_y = -round(z)/2 - screen_h/(2*zoom_y) 改为 world.view_w/view_h 的一半（view_w 就是 dataset.screen_w，不再除 zoom）。C++ 仍按旧公式除以 zoom_x/zoom_y，zoom≠1 时相机目标偏差可达 (1-1/zoom)*view/2，被注视目标明显偏离画面中心，纵向同样偏。 | native/lfw/ui/component/camera_ctrl.cpp:90-104 | 改为 cam_x = pos.x - screen_w/2、cam_y = -0.5*round(pos.z) - screen_h/2，删除 zoom_x/zoom_y 除法。 |
| src/LFW/ui/component/GameModeFSMState.ts | partial | 窗口内唯一行为改动：GameModeFSMState_End 的 enter/leave 查 score_board 由 find_child 改为 search_node（递归查找）。C++ 仍用 find_child，只查直接子节点。现在 score_board 位于 页面/__center/__design 下（3 层），find_child 恒返回空，结算时分数板不会显示/隐藏。 | native/lfw/ui/component/game_mode_fsm_state.cpp:38,43 仍为 node.find_child(u"score_board") | 把 game_mode_fsm_state.cpp 两处 find_child 改成 node.search_node(u"score_board")（uinode.cpp:516 已实现同语义递归查找）。 |
| src/LFW/ui/component/Slider/SliderHandle.ts | missing | 070b579a2 把滑块点击/拖动坐标由常量 MODERN_SCREEN_WIDTH/HEIGHT 改为 world.ui_from_scene，按逻辑视口 dataset.screen_w/h 映射；C++ handle_pointing_event 仍用常量 794/450，宽高比非默认时滑块与 UI 坐标系错位、拖动写入错误值。 | native/lfw/ui/component/slider_handle.cpp:193,200（screen_width()/screen_height() 常量） | 在 C++ World 增加按 dataset.screen_w/h 的 ui_from_scene，并在 handle_pointing_event 用它替换常量换算。 |

## medium

| TS 文件 | 结论 | 问题 | C++ 证据 | 修复建议 |
| --- | --- | --- | --- | --- |
| src/LFW/ui/UINode.ts | partial | TS 新增 follow_screen：cook 时清零 pos，UILayer 建页与分辨率变化时按 view_w/view_h 重设页面尺寸和位置；C++ 既无该字段也无 resize_follow_screen，窗口缩放后页面不跟随。 | native/lfw/ui/uinode.h:51-367 无 follow_screen；native/lfw/ui/uilayer.cpp:64-78 create_page 未做 resize/move；native 全仓 grep 无 follow_screen | UINode 增加 follow_screen 标志，并在 UILayer 建页及视图尺寸变化时按 view_w/view_h 重设页面大小并 move_to(0,0,z)。 |
| src/LFW/ui/UINode.ts | partial | TS move_to 新增 clear_descendants_cache()，连带清空子节点的 global_pos/rect/geo 缓存；C++ move_to 只清自身缓存，父节点移动后子节点缓存坐标不失效。 | native/lfw/ui/uinode.cpp:254-260（move_to 仅 clear_caches 自身） | 在 UINode::move_to 里递归清除 _children 的 _cache_global_pos/_cache_rect/_cache_geo（对应 clear_descendants_cache）。 |
| src/LFW/ui/UILayer.ts | partial | 窗口内 8344a0ae6 在 UILayer 引入 follow_screen：建页时 resize 到 view_w/h 并 move_to(0,0,z)，另加 UILayers.resize_follow_screen 在 screen_w/h 变化时重贴。C++ 完全没有对应逻辑（native 内 grep 不到 follow_screen / resize_follow_screen），分辨率改动后页面仍是 cook 时旧尺寸。 | native/lfw/ui/uilayer.cpp:64-78（create_page 仅 UINode::create，无 resize/move_to）；native/lfw/ui/uilayer.h:98-143 无 resize_follow_screen；grep native 无 follow_screen | 给 UIPushPageOpts/create_page 带上 follow_screen 标志，创建后按 world.view 尺寸 resize 并 move_to(0,0,z)；补 UILayers::resize_follow_screen 并在 screen_w/screen_h 变化回调中调用。 |
| src/LFW/ui/UILayer.ts | partial | 弹窗 z 少加层号偏移：TS 的 _create 统一给节点加 _index，open_modal 再加 MODAL_Z_STEP*(n+1)；C++ 的 set/push/reset 手动加了 _index，唯独 open_modal 没加。层号≥1 时弹窗 z 会低于同层页面，被页面盖住。 | native/lfw/ui/uilayer.cpp:209（只加 MODAL_Z_STEP*(size+1)）；对比 set:109、push:157、reset:137 都加了 _index | open_modal 中改为 curr->set_z(curr->z() + _index + MODAL_Z_STEP*(_modals.size()+1))，与 set/push 保持一致。 |
| src/LFW/ui/UILayer.ts | partial | set() 判断顺序不同：TS 先用 page()?.id 做幂等返回、再 close_modals；C++ 反过来先 close_modals 再判同页。因此每帧重复 set_page 到当前页时，C++ 会把打开的弹窗立刻关掉，TS 保留弹窗。 | native/lfw/ui/uilayer.cpp:90-96（close_modals() 在前，同页判断在后） | 把 close_modals() 挪到「has_id 且 page() 的 id 相同」的幂等 return 之后。 |
| src/LFW/LFW.ts | partial | LFW.save_string/load_string（Cache type='string'、name='saved_string_'+key、encodeUTF8）在 C++ 无对应实现，联机服务器地址的保存/恢复行为丢失。 | native/lfw/lfw.h:399-400 仅有 string/strings，grep 无 save_string/load_string/saved_string；native/lfw/lfw.h:212-214 只有 player_cache_*；native/lfw/ui/component/network_logic.cpp:604、:1160 只用默认地址 u"lfj.gim.ink"，不读取上次地址 | 给 LFW 增加 save_string/load_string（host cache put/get，name 前缀 saved_string_），并在 network_logic 连接成功时保存、初始化地址输入框时读取。 |
| src/LFW/LFW.ts | partial | 新增 ILFWOptions{screen_w,screen_h} 与 dataset 屏幕变化→pointings.set_screen_size + layers.resize_follow_screen；C++ LFW 构造无该选项、无 on_dataset_change 挂钩，UI 页不会随分辨率重排。 | native/lfw/lfw.h:301 LFW(ILfwHost&, bool dev)；native/lfw/lfw.cpp:211-278 构造函数直接建 World(:241) 未传屏幕尺寸、未注册 on_dataset_change；native/lfw/ui/ui_input_handle.cpp:37-38 仅事件时读 dataset 屏幕尺寸，无 UI 重排路径 | 在 LFW/World 构造函数接受 screen_w/screen_h 写入 dataset，并注册 world on_dataset_change 在屏幕尺寸变化时重新布局 UI 层。 |
| src/LFW/ui/cook_ui_info.ts | partial | merge_ui_template 的 undefined 过滤未移植：TS 先构造 remain_defined 跳过 undefined 值再展开，模板字段得以保留；C++ 把 remain 的全部键（含 monostate）复制后无条件 spread，模板的 pos/size 等会被 undefined 覆盖，最终落到默认值。 | native/lfw/ui/cook_ui_info.cpp:157-162（remain 复制所有键）、184-185（spread_into 覆盖）；native/lfw/ui/value_spread.cpp:10-13（不跳过 undefined） | 复制 remain 键时跳过 nullish/undefined 值（或在 spread 后再剔除 undefined），保持模板字段优先于未定义值。 |
| src/LFW/ui/cook_ui_info.ts | partial | alignment 归一化未移植且无消费方：TS cook 用 parse_alignment 把 alignment 归一化后交给 UINode 做四边对齐（内置数据 bg.ui.json/init_demo.ui.json 依赖此字段）；C++ cook 不处理，CookedUIInfo 无 accessor，UINode 也没有任何节点级对齐逻辑，JSON 里的 alignment 实际被忽略。 | native/lfw/ui/cook_ui_info.cpp 无 alignment 处理；native/lfw/data/ui_info.h:107-195 无 alignment accessor；native 无 set_alignment/apply_alignment（仅 xml_to_ui_info 产出该字段，另有独立 Alignment 组件） | 把 parse_alignment 移植到 native cook 并写入 cooked alignment，CookedUIInfo 增 accessor，UINode 启动/更新时按四边规则应用。 |
| src/LFW/ui/component/GamePrepareLogic.ts | partial | start_game 的 cam_x 仍以固定 MODERN_SCREEN_WIDTH(794) 作右上界，且缺少 Math.max(left, right-view_w) 下限夹取；TS 已改为世界当前视宽 view_w(=dataset.screen_w)。分辨率/视宽可变时相机起点会与 TS 不一致。 | native/lfw/ui/component/game_prepare_logic.cpp:627-629（screen_width() 定义于 :40） | 改为 mt->range(left, std::max(left, right - world().dataset.screen_w()))，并删除固定 screen_width() 用法。 |
| src/DittoImpl/renderer/UINodeRenderer.ts | partial | 窗口内新增的输入框占位提示未移植：TS 把 props.placeholder 与 set_input_placeholder 写进 DOM input，C++ 的 TextInput/LineEdit 链路完全没有 placeholder 概念，联机房间列表的 network.pwd_hint 提示在 C++ 侧不显示。 | native/lfw/ui/component/text_input.cpp:16-25 只读 maxLength/defaultValue/fontSize/fontFamily；native/host/godot/lfw_godot_renderer.h:142-153 GodotTextInputBox 无 placeholder 字段；native/host/godot/lfw_godot_text_input.cpp:96-121 创建 LineEdit 从不断言 set_placeholder；native/lfw/ui/component/network_logic.cpp:653-657 绑定房间密码框只挂 on_submit，全程无 network.pwd_hint 键使用 | 给 ui::TextInput 增加 placeholder()（读 props.placeholder）与 GodotTextInputBox.placeholder 字段，在 GodotTextInputs::sync 里调用 LineEdit::set_placeholder，并在 network_logic 的房间行密码框上设置 t("network.pwd_hint")。 |
| src/LFW/Camera.ts | partial | TS 已删除 view_h 的 zoom_y 除法，且 acc_y 分母同样用 view_h；C++ 的 view_h 仍除以 zoom_y，而 acc_y 分母却用 screen_h，zoom_y≠1 时纵向缓动与上限与 TS 不一致。 | native/lfw/camera.cpp:113-115（zoom_y 除法保留）、123（分母 screen_h） | view_h 取 dataset.screen_h 不再除 zoom_y，acc_y 分母改用 view_h。 |
| src/LFW/Camera.ts | partial | TS 横向上限加了 max(min_cam_x,...) 保底并去掉 /zoom_x；C++ 仍是 cam_r - screen_w/zoom_x 且无 max 保底，地图窄于视口时上下界倒置，clamp 会把 destination.x/position.x 压到 cam_l 之外。 | native/lfw/camera.cpp:73-75 | 改为 max(min_cam_x, cam_r - dataset.screen_w())，保留 min_cam_x 兜底。 |
| src/LFW/cmds/CMD_F4.ts | partial | modal.ignore_esc 语义缺失：TS 中顶层弹窗 ignore_esc 为真时仍 break，即 F4 整条命令什么都不做、绝不下探到更低层；C++ 无此提前终止，会继续向下找层并弹页面。 | native/lfw/cmds/cmd_f4.cpp:30-31（只有 layer.ui().ignore_esc 的 continue，无弹窗级阻断） | 弹窗分支中无论 ignore_esc 真假都要 break，只在为假时才调用 close_modal。 |
| src/LFW/Ground.ts | partial | TS 删除 Ground.block() 并改写受阻推挤逻辑，C++ 仍保留 block() 并沿用旧四象限推挤。 | native/lfw/ground.cpp:121-200 仍保留 block() 四象限推挤；native/lfw/world.cpp:808-824 仍调用 ground.block()，末尾 native/lfw/world.cpp:822 设置 restrict_result_.y = Ground::y(seg, x, z) | 按 TS 重写 native/lfw/world.cpp restrict 的受阻分支：按上一帧所在侧退回并 clamp，y 保持原值，移除 ground.block() 调用。 |
| src/LFW/ui/component/MiscSettingsLogic.ts | missing | on_settings_change 回调缺 render_w/render_h 分支（应重新同步分辨率控件值）；C++ 仍监听 on_dataset_change 且只处理 sync_render，未随 TS 事件改名迁移。 | native/lfw/ui/component/misc_settings_logic.cpp:234-243 仅 a.key==sync_render | 改为监听 on_settings_change，键为 sync_render 或 render_w/render_h，后者调用 sync_render_resolution。 |
| src/LFW/ui/component/MiscSettingsLogic.ts | missing | on_resume 末尾未同步分辨率控件值；缺少 sync_render_resolution helper（按 render_resolution_index 取下标，越界回退 0）。 | native/lfw/ui/component/misc_settings_logic.cpp:347-371 on_resume 以 lang_row 结束，无该调用与 helper | 新增 sync_render_resolution()，并在 on_resume 末尾调用，idx<0 时 set_value(0)。 |
| src/LFW/ui/component/MiscSettingsLogic.ts | missing | sync_render 存储目标未随 491809c12 迁移到 World.settings（客户端本地、非联机同步、可 dump），C++ 仍读写 WorldDataset；native 无 Settings 概念。 | native/lfw/ui/component/misc_settings_logic.cpp:302、363 用 dataset.set/sync_render；native/lfw/world.cpp:1193、1223 从 dataset 读取 | 引入 native Settings（含 sync_render/render_w/render_h 与变更回调），或显式确认沿用 dataset 并保持渲染重启语义。 |
| src/LFW/ui/action/Actor.ts | partial | Actor.ts:23（c4e4ffb72, 2026-11-13）把 pop_page 动作的 min_pages 改为按所在层 index 取值：非 0 层为 0（允许弹空），0 层为 1（至少留一页）；C++ actor.cpp:48 仍固定 1.0，非 0 层（如 chat/overlay 层）只剩一页时 TS 能弹空、C++ 拒绝，行为不一致。 | native/lfw/ui/action/actor.cpp:46-50，关键行 48 `opts.min_pages = 1.0;`（不区分层）；UINode::pop_page 直接把 opts 传给 layer->pop（native/lfw/ui/uinode.cpp:1020），UILayer::pop 按 min_pages 截断（native/lfw/ui/uilayer.cpp:166-172），层 index 取值器见 native/lfw/ui/uilayer.h:55。 | 在 pop_page 处理器里按层号计算 min_pages：`UILayer* l = n.layer(); opts.min_pages = (l != nullptr && l->index() != 0.0) ? 0.0 : 1.0;`。 |
| src/LFW/ui/component/DemoModeLogic.ts | partial | 视口宽度公式未同步：TS 现用 world.view_w（即 dataset.screen_w，已不再除背景 zoom），C++ 仍用 screen_w/zoom_x（窗口前的旧式），导致 VS 模式的 cam_x、min_x、max_x 计算在背景 zoom_x≠1 时不同，进而改变角色出生 X 与相机初始位置。 | native/lfw/ui/component/demo_mode_logic.cpp:459-465 `const double zx = bg->zoom_x(); ... const double view_w = world().dataset.screen_w() / use_zx;` 并用于 cam_x/min_x/max_x（:463-465）。 | 把 demo_mode_logic.cpp:461 的 view_w 改为 world().dataset.screen_w()（或在 C++ World 上补 view_w 访问器后改用它），删除 zoom_x 除法，使 cam_x/min_x/max_x 与 TS 一致。 |
| src/LFW/ui/component/TextInput.ts | partial | 窗口内新增的 placeholder 属性在 C++ 完全没有落地：无访问器、宿主也不给 LineEdit 设 placeholder，原生输入框提示文字会缺失。 | native/lfw/ui/component/text_input.h:22-25 只有 max_length/default_value/font_size/font_family；native/lfw/ui/component/text_input.cpp:16-26 无 placeholder()；native/host/godot/lfw_godot_renderer.h:142-153 GodotTextInputBox 无 placeholder 字段；native/host/godot/lfw_godot_text_input.cpp:94-121 只设字体/颜色/长度，从无 set_placeholder | 在 text_input.h/.cpp 加 placeholder() 读 props_holder.str(u"placeholder")，给 GodotTextInputBox 加 placeholder 字段，并在 sync 中调用 LineEdit::set_placeholder（纳入 style_sig 以防每帧重设）。 |
| src/LFW/WorldDataset.ts | partial | 窗口新增两个字符串字段 weapon_rain_oids / weapon_rain_groups（默认空串）未进 C++ 默认值表：get 返回 undefined、tracked() 为 false、dump/XML 丢该键（reset_world_dataset 会写 undefined），与 TS 的 '' 默认不一致。 | native/lfw/world_dataset.cpp:37-149 install_defaults 无这两个键；native/lfw/world_dataset.h:20-126 的字段宏只收 double；而 native/lfw/defines/fields_gen.h:231 的 world_dataset_fields 已含这两键，dump_dataset(native/lfw/world_dataset.cpp:194-205) 会按表取到 monostate | 在 install_defaults 里以 Value(u"") 注册 weapon_rain_oids/weapon_rain_groups 并纳入 tracked()，使 get/dump/reset/callback 与 TS 对齐。 |
| src/LFW/WorldDataset.ts | partial | 窗口内 TS 已把 sync_render 从 WorldDataset 删除并移入 Settings，C++ WorldDataset 仍保留该字段，world.cpp 仍从 dataset 读取渲染帧率，导致 dataset 字段集/dump 与 TS 源不一致（C++ 侧也没有 Settings 承接）。 | native/lfw/world_dataset.h:72；native/lfw/world_dataset.cpp:90 add(u"sync_render",3)；消费方 native/lfw/world.cpp:606、1193、1223 仍读 dataset.sync_render() | 从 C++ WorldDataset 移除 sync_render，并仿照 Settings.ts 增加 settings 存储供 world.cpp 读取渲染帧率。 |
| src/LFW/dat_translator/xml/xml_x_bg_layer.ts | partial | xml_2_bg_layer 未解析 center：TS 在 1f587a300 于 width_percent 之后新增该字段解析与默认值保留，C++ 解析缺该项，center 在 C++ 侧读取后丢失。 | native/lfw/dat_translator/xml/xml_x_bg_layer.cpp:45 | 在 width_percent 之后补 center 解析（get_num("center") 回退 bg_layer_info_new 默认值）。 |
| src/LFW/dat_translator/xml/xml_x_bg_layer.ts | partial | xml_x_bg_layer 未写回 center 属性：TS 在 width_percent 与 uv_loop 之间 set_attr，C++ 输出 XML 缺 center，破坏编辑器/运行时往返。 | native/lfw/dat_translator/xml/xml_x_bg_layer.cpp:75 | 在 uv_loop 之前补 set_attr(u"center", field_or(l, u"center"))。 |
| src/LFW/defines/defines.ts | missing | DEFAULT_RENDER_RESOLUTION 缺失：新增 aspect:"16:9"，label 由 "3176×1800" 变为 "3176×1800 (16:9)"；TS 的 Settings.render_w/render_h 默认值取自它，C++ 既无该常量也无这两个设置项。 | native/lfw/defines/runtime_gen.cpp:136-140（仅 DEFAULT_* 数值条目） | 重跑生成器补 DEFAULT_RENDER_RESOLUTION，并在设置中补 render_w/render_h 及默认值。 |
| src/LFW/defines/defines.ts | missing | render_resolution_index(w,h) 缺失：按 w/h 精确匹配预设，不匹配返回 -1（设置页把 -1 落到 0）。C++ MiscSettingsLogic 没有 render_resolution_row，也没有该索引与当前分辨率的同步回调。 | native/lfw/ui/component/misc_settings_logic.cpp:299-305（只有 render_rate 行） | 补 render_resolution_index，并实现设置页该行的取值/写回同步。 |
| src/LFW/state/CharacterState_Base.ts | partial | on_leave_ground 新增的 StateEnum.Defend 分支未同步：TS 中防御状态离地（如防御+下穿透地形）会进入 auto 帧转坠落，C++ 只匹配 Running/Walking/Standing/Rowing，Defend 离地后仍停在防御帧。 | native/lfw/state/character_state_base.cpp:96-101 | 在 csb_on_leave_ground 的状态判断中加入 StateEnum::Defend。 |
| src/LFW/ui/component/DanmuGameLogic.ts | partial | 窗口内两次提交把可见宽度公式从 dataset.screen_w/(bg.zoom_x\|\|1) 改为 world.view_w（即 dataset.screen_w，不再除 zoom）。C++ 两处仍按 screen_w/zoom_x 计算 view_w，zoom_x≠1 时镜头 x、DIST_CAM 值以及 update_bg 的左右夹取边界都与 TS 不同。 | native/lfw/ui/component/danmu_game_logic.cpp:374-375,841-842 | 用 const double view_w = world().dataset.screen_w(); 去掉 zoom_x 除法，与 World.view_w 对齐。 |
| src/LFW/ui/component/PlayerNameInput.ts | partial | c5cd3fa99 删除了 maxLength getter（原 props_holder.num('maxLength') ?? 10），TS 侧 PlayerNameInput 无 props 时不再有 10 字上限（DOM renderer 无 maxLength 即 removeAttribute）；C++ 仍保留 max_length() 返回 value_or(10.0)，godot 宿主据此把 LineEdit 限到 10 字，设置界面里的 PlayerNameInput() 仍只能输入 10 字。 | native/lfw/ui/component/player_name_input.cpp:22-24（消费方 native/host/godot/lfw_godot_text_input.cpp:94,120） | 删除 PlayerNameInput::max_length() override（回落 TextInput 的 props.maxLength，不带 10 默认值）。 |
| src/LFW/ui/component/ReachableGroup.ts | partial | 窗口内唯一行为性改动：binded_layout 由 node.root.find_child(lid) 改为 node.root.search_node(lid)，即从“仅查直接子节点”变为“深度优先递归查找后代”。C++ 仍用 find_child，仅能命中直接子节点；当 args[2] 指向更深层后代时 C++ 找不到而回退到自身 node，导致 on_key_down 的 visible/disabled 门控作用于错误节点（可能错误放行或屏蔽方向键）。 | lfw/ui/component/reachable_group.cpp:32-37（第35行 find_child）；native/lfw/ui/uinode.cpp:516-524 已有等价 search_node 可复用 | binded_layout() 中把 node.root().find_child(lid) 改为 node.root().search_node(lid)。 |
| src/LFW/ui/component/ScrollView.ts | partial | 指针 scene→UI 坐标换算未同步：TS 改用 world.ui_from_scene（按运行时视口 screen_w/screen_h 换算），C++ 仍用固定 MODERN_SCREEN 常量。视口尺寸不等于 794×450 时，内容拖拽、滚轮滚动、滚动条轨道/thumb 命中判定会与宿主（UiInputHandle 用 dataset 视口尺寸做命中）不一致，偏移量与差值成正比。 | native/lfw/ui/component/scroll_view.cpp:455-478，常量助手在 :25-26 | 在 pointer_axis_px/pointer_in_view/pointer_in_node 内改用 world().dataset.screen_w()/screen_h() 换算（等价 host::ui_point_of_scene），删除 screen_width()/screen_height() 常量助手。 |
| src/LFW/ui/component/Slider/ArrowSwitcher.ts | missing | 窗口内唯一行为性改动是为 ArrowSwitcher 新增运行时改 items 的 API：`set items` 赋值器 + set_items(v: readonly string[])。它把数组 join 成逗号串写回 props.items，把 _value 钳制到 max(0, len-1)，并立即 sync_label()。C++ 只有只读 items()，没有 set_items/写 props 的入口，运行时无法追加或替换选项；且 C++ 的 _value 不会被写入钳制（仅 value() 读取时 min），选项先缩小再扩大后 TS 停在钳制后的下标、C++ 会恢复旧下标，标签也会在下次同步前保持旧值。 | native/lfw/ui/component/arrow_switcher.h:40-52（无 set_items）；native/lfw/ui/component/arrow_switcher.cpp:73-83, 124-134 | 在 ArrowSwitcher 增加 set_items(const std::vector<std::u16string>&)：逗号 join 后 props_holder.set_str(u"items")，将 _value 钳制到 max(0, n-1) 再调用 sync_label()。 |

## 逐文件结论

### src/LFW/ui/UINode.ts — partial（28 次提交）

- [high] TS 新增节点级 alignment（字段 + set_alignment + 每帧 apply_alignment/once 一次性），C++ UINode 完全没有该字段与更新钩子，cook 出的 alignment 值无消费方，UI XML 里 alignment= 的节点不会被布局。
  - TS：src/LFW/ui/UINode.ts:82（alignment 字段）、379-383（set_alignment）、834-837（update 调 apply_alignment，once 置 _alignment_done）；提交 8152622ff
  - C++：native/lfw/ui/uinode.h:51-367、native/lfw/ui/uinode.cpp:669-698（update 无 alignment）；native/lfw/ui/xml_to_ui_info.cpp:792 只写入 alignment，native 无任何读取点
  - 修复：在 UINode 增加 alignment 字段与 set_alignment，并在 update() 开头按 alignment.ts 的 apply_alignment 语义对自身执行一次（返回 true 且 once 时置 _alignment_done）。
- [medium] TS 新增 follow_screen：cook 时清零 pos，UILayer 建页与分辨率变化时按 view_w/view_h 重设页面尺寸和位置；C++ 既无该字段也无 resize_follow_screen，窗口缩放后页面不跟随。
  - TS：src/LFW/ui/UINode.ts:81（follow_screen 字段，提交 8344a0ae6）；UILayer.ts:168-175、357-370（建页与 resize_follow_screen）
  - C++：native/lfw/ui/uinode.h:51-367 无 follow_screen；native/lfw/ui/uilayer.cpp:64-78 create_page 未做 resize/move；native 全仓 grep 无 follow_screen
  - 修复：UINode 增加 follow_screen 标志，并在 UILayer 建页及视图尺寸变化时按 view_w/view_h 重设页面大小并 move_to(0,0,z)。
- [medium] TS move_to 新增 clear_descendants_cache()，连带清空子节点的 global_pos/rect/geo 缓存；C++ move_to 只清自身缓存，父节点移动后子节点缓存坐标不失效。
  - TS：src/LFW/ui/UINode.ts:406-421（move_to 调 clear_descendants_cache）、423-430；提交 8152622ff
  - C++：native/lfw/ui/uinode.cpp:254-260（move_to 仅 clear_caches 自身）
  - 修复：在 UINode::move_to 里递归清除 _children 的 _cache_global_pos/_cache_rect/_cache_geo（对应 clear_descendants_cache）。
- [low] TS hit() 由 data.size 改为实时 size；C++ 仍用 cook 快照 info().size()，resize（flex/alignment/文本自适应）后命中范围不跟随。当前 native 无调用点，属潜在语义偏差。
  - TS：src/LFW/ui/UINode.ts:619-626（l + dw，提交 1df83c535）
  - C++：native/lfw/ui/uinode.cpp:360-366（dw/dh 取 info().size()）
  - 修复：hit() 中 width/height 改用 size.x/size.y，而非 info().size()。
- [low] TS 的 update() 对每个子节点先 flush_flex() 再按 disabled 决定是否 update；C++ 只在子节点未 disabled 时 update，禁用期间的 flex 容器不会重排。
  - TS：src/LFW/ui/UINode.ts:871-874
  - C++：native/lfw/ui/uinode.cpp:695-697
  - 修复：子节点循环里先 child->flush_flex()，再按 !child->disabled() 决定是否 child->update(dt)。

> 备注：窗口内 UINode.ts 共 28 个提交（a59991a6e..8152622ff，diff 255+/136-）。已同步的行为：按鼠标键的 _pointer_down 数组与 is_pointer_down、callbacks 公开化、link_singleton/fire_singleton_links 与 on_stop 的 on_unlink+清空、flex 脏标记传播（is_flex/invalidate_flex/flush_flex/on_flex_change、resize/set_center/add_child/set_visible 钩子、update 与 on_resume flush）、set_text 去 style 参数与同文本短路、set_i18n/resolve_text/refresh_text_layout/refresh_lang_text、stretchable 取代 raw.size 判定、touchable/local_only/ignore_key/ignore_esc、pop_page 的 modal 分支、_attach_components 与 create/fire/flush 顺序。任务给的路径 lfw/ui/uinode.cpp\|.h 实际位于 native/lfw/ui/ 下（仓库根无该目录）。对齐组件（component/alignment.*）只是 TS component/alignment.ts 的移植，不能替代 UINode 节点级 alignment。

### src/LFW/ui/UILayer.ts — partial（18 次提交）

- [medium] 窗口内 8344a0ae6 在 UILayer 引入 follow_screen：建页时 resize 到 view_w/h 并 move_to(0,0,z)，另加 UILayers.resize_follow_screen 在 screen_w/h 变化时重贴。C++ 完全没有对应逻辑（native 内 grep 不到 follow_screen / resize_follow_screen），分辨率改动后页面仍是 cook 时旧尺寸。
  - TS：src/LFW/ui/UILayer.ts:163-177（_create 的 follow_screen 分支）、357-370（resize_follow_screen）；提交 8344a0ae6、5bd6a5635；数据来源 src/LFW/ui/cook_ui_info.ts:303-304
  - C++：native/lfw/ui/uilayer.cpp:64-78（create_page 仅 UINode::create，无 resize/move_to）；native/lfw/ui/uilayer.h:98-143 无 resize_follow_screen；grep native 无 follow_screen
  - 修复：给 UIPushPageOpts/create_page 带上 follow_screen 标志，创建后按 world.view 尺寸 resize 并 move_to(0,0,z)；补 UILayers::resize_follow_screen 并在 screen_w/screen_h 变化回调中调用。
- [medium] 弹窗 z 少加层号偏移：TS 的 _create 统一给节点加 _index，open_modal 再加 MODAL_Z_STEP*(n+1)；C++ 的 set/push/reset 手动加了 _index，唯独 open_modal 没加。层号≥1 时弹窗 z 会低于同层页面，被页面盖住。
  - TS：src/LFW/ui/UILayer.ts:167（_create 内 ret.z + this._index）、271-284（open_modal 在其后再加 MODAL_Z_STEP*(_modals.length+1)）
  - C++：native/lfw/ui/uilayer.cpp:209（只加 MODAL_Z_STEP*(size+1)）；对比 set:109、push:157、reset:137 都加了 _index
  - 修复：open_modal 中改为 curr->set_z(curr->z() + _index + MODAL_Z_STEP*(_modals.size()+1))，与 set/push 保持一致。
- [medium] set() 判断顺序不同：TS 先用 page()?.id 做幂等返回、再 close_modals；C++ 反过来先 close_modals 再判同页。因此每帧重复 set_page 到当前页时，C++ 会把打开的弹窗立刻关掉，TS 保留弹窗。
  - TS：src/LFW/ui/UILayer.ts:180-184（if (is_str(id) && this.page?.id === id) return; 在 close_modals() 之前）
  - C++：native/lfw/ui/uilayer.cpp:90-96（close_modals() 在前，同页判断在后）
  - 修复：把 close_modals() 挪到「has_id 且 page() 的 id 相同」的幂等 return 之后。
- [low] pop() 生命周期顺序不同：TS 先把弹出的页从栈中移除，再对该批节点 on_pause/on_stop，最后 ui.on_resume；C++ 在 erase 之前就调用 on_pause/on_stop，回调期间 layer.ui() 仍指向被弹出的节点。
  - TS：src/LFW/ui/UILayer.ts:247-255（先 splice，再 pause/stop，再 ui.on_resume）
  - C++：native/lfw/ui/uilayer.cpp:187-195（on_pause/on_stop 在 erase 与 _trash 转移之前）
  - 修复：先 erase 弹出项（移入 _trash 保活）再触发 on_pause/on_stop，使回调中 ui() 已是新栈顶。
- [low] UILayers 层缺少 has_modal(id) 重载：TS 新增可带 id 的全局查询（Web 宿主用它判断 network_modal 是否打开），C++ 只有无参 has_modal()，只能逐层调 UILayer::has_modal(id)。
  - TS：src/LFW/ui/UILayer.ts:399-401；调用方 src/App.tsx:860
  - C++：native/lfw/ui/uilayer.h:121（仅 bool has_modal() const）；native/lfw/ui/uilayer.cpp:385-390
  - 修复：给 UILayers 增加 has_modal(const std::u16string& id) 重载，遍历 _all 转调 layer->has_modal(id)。
- [low] dispose() 未清空页面栈：TS 的 dispose 在 pause/stop 后把 pages 与 _modals 都清空（ui() 变 undefined）；C++ 只清 _modals，_pages 保留，dispose 后 ui()/pages() 仍返回节点。
  - TS：src/LFW/ui/UILayer.ts:153-160（this.pages.length = 0; this._modals.length = 0）
  - C++：native/lfw/ui/uilayer.cpp:80-87（只 _modals.clear()）；间接调用方 native/lfw/lfw.cpp:639
  - 修复：dispose 末尾同时清空 _pages（移入 _trash 或直接 clear），使 dispose 后 ui()/pages() 为空。

> 备注：窗口内 18 个提交（a59991a6e..af1909373），用 a59991a6e^..HEAD 折叠 diff 比对。已同步且逐项核对一致：MODAL_Z_STEP=0.25（h:49）、_modals 栈与 page/modal/modals/has_modal 语义（cpp:27-44）、live_count/live_at 公式（cpp:46-55，world.cpp:852-857 在用）、reset 整层重置与回调（cpp:121-147）、push 先 close_modals（cpp:151）、pop 的 min_pages/until/inclusive 与 bool 返回（cpp:166-198）、open_modal 同 id 幂等（cpp:201-205）、close_modal 自顶向下关到 id 含自身（cpp:219-242）、close_modals 取最底层 id（cpp:244-254）、UILayers.pages→layer_tops（cpp:392-400 等价自顶向下）、on_modal_open/on_modal_close 回调已接线（lfw.cpp:259-265）。未计入 finding：UILayers::shape/generation、_trash 保活、ensure 对非整数层的 _loose 处理、create_page 缺失时的 warn，均为 C++ 侧额外实现或日志，不属于 TS 行为缺失。dispose 未清 _pages 目前仅在 UILayers::dispose 后随 _all 析构，实际不可观测，故记为 low。

### src/LFW/LFW.ts — partial（17 次提交）

- [medium] LFW.save_string/load_string（Cache type='string'、name='saved_string_'+key、encodeUTF8）在 C++ 无对应实现，联机服务器地址的保存/恢复行为丢失。
  - TS：src/LFW/LFW.ts:941-952（commit c5cd3fa99 新增）；调用方 src/LFW/ui/component/network/NetworkModalLogic.ts:33、:100
  - C++：native/lfw/lfw.h:399-400 仅有 string/strings，grep 无 save_string/load_string/saved_string；native/lfw/lfw.h:212-214 只有 player_cache_*；native/lfw/ui/component/network_logic.cpp:604、:1160 只用默认地址 u"lfj.gim.ink"，不读取上次地址
  - 修复：给 LFW 增加 save_string/load_string（host cache put/get，name 前缀 saved_string_），并在 network_logic 连接成功时保存、初始化地址输入框时读取。
- [medium] 新增 ILFWOptions{screen_w,screen_h} 与 dataset 屏幕变化→pointings.set_screen_size + layers.resize_follow_screen；C++ LFW 构造无该选项、无 on_dataset_change 挂钩，UI 页不会随分辨率重排。
  - TS：src/LFW/LFW.ts:74-77（ILFWOptions）、:386、:408-417（new World(this, options)、set_screen_size、resize_follow_screen；commit 070b579a2 / 8344a0ae6）
  - C++：native/lfw/lfw.h:301 LFW(ILfwHost&, bool dev)；native/lfw/lfw.cpp:211-278 构造函数直接建 World(:241) 未传屏幕尺寸、未注册 on_dataset_change；native/lfw/ui/ui_input_handle.cpp:37-38 仅事件时读 dataset 屏幕尺寸，无 UI 重排路径
  - 修复：在 LFW/World 构造函数接受 screen_w/screen_h 写入 dataset，并注册 world on_dataset_change 在屏幕尺寸变化时重新布局 UI 层。
- [low] DEFAULT_INFO 的 title/description 已改为 Legacy Flame War，C++ default_info 仍是 Little Fighter Wemake 旧文案。
  - TS：src/LFW/LFW.ts:69-70（commit e179b67e4）
  - C++：native/lfw/lfw.cpp:49-50 default_info() 中 title/description 仍为 u"Little Fighter Wemake Origin Full Game(Zip)"
  - 修复：把 native/lfw/lfw.cpp:49-50 两条默认文案同步为 Legacy Flame War Origin Full Game / ... Zip。
- [low] t(name, ...args) 的 %N 占位替换是 LFW 通用 API；C++ 只有 string/strings，替换仅散落在 network_logic 的局部 t1（且只处理 %1）。
  - TS：src/LFW/LFW.ts:813-818（commit cec7c610c，由 string_with_args 合并而来）
  - C++：native/lfw/lfw.h:399-400 无 t/带参 string；native/lfw/ui/component/network_logic.cpp:259-266 t1 仅 replace %1
  - 修复：在 LFW 增加 t(name, args...) 按顺序替换 %1..%N 的通用实现，供各 UI 组件复用。

> 备注：窗口内其余行为改动已同步：push_lcmd/lcmds 与 clear_lcmds（lfw.cpp:580-588,1181-1184）、mounted_keys/regist_keys/recycle_keys（lfw.cpp:880-913）、load 跳过已加载包 loaded_before（lfw.cpp:1666-1691）、game_mode 取代 survival_rank_mode/2p（lfw.h:283,525 + game_mode.h）、UILayers on_modal_open/close→on_ui_changed（lfw.cpp:259-265）、host_node/单例（lfw.cpp:590-626）、events 改为 KEY_EVENT 命令（lfw.cpp:494,559）、set_lang 重解析（lfw.cpp:706-734）。lang_version 仅 Web 渲染器使用（UINodeRenderer 按版本重解析），Godot 渲染器每帧 lfw.string(text) 重解析（lfw_godot_ui_renderer.cpp:359-364），按 Web 专属不判缺失。

### src/LFW/ui/cook_ui_info.ts — partial（15 次提交）

- [high] 窗口新增的 touchable 字段完全未移植：TS 在 cook 时解析 touchable 并经 UINodeBuilder 写入节点，用于指针命中判定；C++ 的 cook_ui_info、CookedUIInfo、UINode 构造都没有 touchable，native 全仓搜不到 u"touchable"，UI 数据里写 touchable:false 不生效，浮层仍会吃掉指针。
  - TS：src/LFW/ui/cook_ui_info.ts:186,210（提交 4680beeeb）；消费方 src/LFW/ui/UINodeBuilder.ts:57、src/LFW/ui/UINode.ts:229、src/DittoImpl/ui/UIInputHandle.ts:135
  - C++：native/lfw/ui/cook_ui_info.cpp:318-350（初始化/赋值块无 touchable）；native/lfw/data/ui_info.h:121-130（无 accessor）；native/lfw/ui/uinode.cpp:75-86（构造不读 touchable）；native 内无 u"touchable" 匹配
  - 修复：在 cook_ui_info.cpp:343 附近按 local_only 的模式解析 touchable，CookedUIInfo 增设 accessor，UINode 构造时 set_touchable(_info.touchable())，未设时沿用父节点。
- [high] 根页 follow_screen 逻辑缺失：TS 根节点尺寸取 view_w/view_h（缺省 794/450），仅当非 flex 自适应时 follow_screen=true 并把 pos 重置为 (0,0,z)；C++ 根分支无条件用 dataset screen_w/h 撑满且不重置 pos，也没有任何 follow_screen 消费方，分辨率变化时根页不会重新适配。
  - TS：src/LFW/ui/cook_ui_info.ts:300-305（提交 070b579a2、8344a0ae6、af1909373）；消费方 src/LFW/ui/UILayer.ts:168-175、357-365、src/LFW/ui/UINodeBuilder.ts:63
  - C++：native/lfw/ui/cook_ui_info.cpp:674-681（仅 dataset.get(screen_w/h)，无 follow_screen、无 pos 重置）；native 内无 follow_screen 匹配；screen_w 只在 cook 时读取
  - 修复：根分支按 ret.flex 的 display/fit_w/fit_h 计算 follow_screen，尺寸用 view 尺寸（dataset 缺省 794/450），follow_screen 为真时 pos=[0,0,pos.z]，并在 UILayer 提供屏幕变化时按 follow_screen 重排。
- [medium] merge_ui_template 的 undefined 过滤未移植：TS 先构造 remain_defined 跳过 undefined 值再展开，模板字段得以保留；C++ 把 remain 的全部键（含 monostate）复制后无条件 spread，模板的 pos/size 等会被 undefined 覆盖，最终落到默认值。
  - TS：src/LFW/ui/cook_ui_info.ts:47-50,53（提交 1caa9235e）
  - C++：native/lfw/ui/cook_ui_info.cpp:157-162（remain 复制所有键）、184-185（spread_into 覆盖）；native/lfw/ui/value_spread.cpp:10-13（不跳过 undefined）
  - 修复：复制 remain 键时跳过 nullish/undefined 值（或在 spread 后再剔除 undefined），保持模板字段优先于未定义值。
- [medium] alignment 归一化未移植且无消费方：TS cook 用 parse_alignment 把 alignment 归一化后交给 UINode 做四边对齐（内置数据 bg.ui.json/init_demo.ui.json 依赖此字段）；C++ cook 不处理，CookedUIInfo 无 accessor，UINode 也没有任何节点级对齐逻辑，JSON 里的 alignment 实际被忽略。
  - TS：src/LFW/ui/cook_ui_info.ts:201（提交 8152622ff）；消费方 src/LFW/ui/UINodeBuilder.ts:64、src/LFW/ui/UINode.ts:834；数据 public/builtin_data/launch/bg.ui.json:5-10
  - C++：native/lfw/ui/cook_ui_info.cpp 无 alignment 处理；native/lfw/data/ui_info.h:107-195 无 alignment accessor；native 无 set_alignment/apply_alignment（仅 xml_to_ui_info 产出该字段，另有独立 Alignment 组件）
  - 修复：把 parse_alignment 移植到 native cook 并写入 cooked alignment，CookedUIInfo 增 accessor，UINode 启动/更新时按四边规则应用。
- [low] size 默认值语义未同步：TS 把初值由 [0,0,0] 改为 undefined，且图片/文本尺寸推导前加 if(ret.size) 守卫，未声明尺寸的子节点保持“无尺寸”；C++ 仍初始化 zeros3 并无条件把 img/txt 测得尺寸写入该数组。影响被 UINode 用 raw 判定 stretchable 部分抵消。
  - TS：src/LFW/ui/cook_ui_info.ts:173、312-322（提交 7ad152c2b）
  - C++：native/lfw/ui/cook_ui_info.cpp:332（size=zeros3）、700-720（无条件改 size）；补偿点 native/lfw/ui/uinode.cpp:83-86
  - 修复：size 缺失时保持 undefined 不写数组，并在推导 img/txt 尺寸前判断 size 是否存在（或保留现状但在 cook 侧显式记录“未声明尺寸”）。

> 备注：路径更正：任务给的 lfw/ui/cook_ui_info.cpp\|.h 实际位于 native/lfw/ui/（仓库根无 lfw/ 目录），已按 native/ 路径审计。窗口内 15 个提交，行为性改动集中在 7 个：f4998df7b/f49666cb7(local_only)、a8601c43c(ignore_esc)、4680beeeb(touchable)、1df83c535(flex 归一化)、77f789a88(组件自带 items)、7ad152c2b(size 语义)、070b579a2/8344a0ae6/af1909373(根页尺寸/follow_screen)、8152622ff(alignment)、1caa9235e(undefined 过滤)、cec7c610c(i18n)、4476e5fa6(移除 raw)。已确认已同步或等价：local_only、ignore_esc（cook_ui_info.cpp:343-344,404-411）、组件 ITEMS 注入与 properties 过滤（cook_ui_info.cpp:281-306）、flex 归一化（由 FlexStyle::assign 在 uinode.cpp:90 消费，等价 normalize_flex_style）、i18n 翻译（native LFW::string 即 i18n 查表，等价 TS lfw.t）。4476e5fa6 移除 raw 属 C++ 多保留字段（其 uinode.cpp:83 反而依赖 raw），不计为缺失。纯 TS 类型/导入/React/DOM 重构（6f8583787 等）不计。native 基线提交 8d6b4fe53（2026-10-08）早于窗口内改动，故部分已人工补植、部分遗漏。

### src/DittoImpl/ui/UIInputHandle.ts — synced（11 次提交）

- [low] 指针射线相机从 world_renderer.camera 改为 ui_fg_camera，C++ 无对应概念。但两者投影参数与每帧位置完全相同（WorldRenderer.ts:281-289、323-333），当前为语义无变化的重构；C++ 侧命中检测不走射线，无需移植。
  - TS：src/DittoImpl/ui/UIInputHandle.ts:125（commit 7993f1d56）；src/DittoImpl/renderer/WorldRenderer.ts:204-207,281-289,323-333
  - C++：native/host/common/ui_input_handle.cpp:42-45（ui_point_of_scene 纯 2D 映射）、156-207（矩形命中，仅用 world().camera().position 求命中点）
  - 修复：无需修改；若将来要跟随渲染端插值相机，可在 Hit.x/y 中用渲染相机的插值位置替换 world().camera()。

> 备注：窗口内提交：d59dc6e73, dc4a1dfce, d0ede083c, e12ab0fa7, 8f6740320, 43457ffec, 1461c4a93, 65ce6b142, d0b7c6c91, 070b579a2, 7993f1d56（净差异 d59dc6e73^..HEAD）。逐项核对：(1) 新增 touchable 门禁（TS:135，dc4a1dfce→e12ab0fa7→8f6740320 最终为 !ui.touchable）↔ C++:174 要求 node.touchable()，且 native/lfw/ui/uinode.cpp:598-601 默认 true/继承父节点与 TS UINode.ts:229 一致；(2) 移除 --pages 参数（43457ffec）↔ C++:277-282 从不发 --pages，接收端 cmd_pointer_events.cpp:139-150 只解析 --paths/--points；(3) local_only 命中分流为 push_cmd/push_lcmd（65ce6b142,d0b7c6c91）↔ C++:241-255,276-283 完全一致（无 local 命中只发 lcmd，无命中不发）；(4) 删除 _empty_move/push_empty_move 空移动节流，改为空命中不发任何命令 ↔ C++ 无该字段、:258-283 空命中仅在不含 local 且为 POINTER_DOWN 时提前 return，与 TS:108-120 等价；(5) 命中页选择改为遍历全部层顶层页取首个有命中者（dc4a1dfce 起，1461c4a93/65ce6b142 定型）↔ C++:215-225 用 UILayers::layer_tops()（uilayer.cpp:392-400，顶层到底层、每层取 ui），与 TS UILayers.pages（UILayer.ts:325-332）同序同集合；(6) pos_arg/update_local_cursor 改用 world…

### src/LFW/ui/component/GamePrepareLogic.ts — partial（11 次提交）

- [medium] start_game 的 cam_x 仍以固定 MODERN_SCREEN_WIDTH(794) 作右上界，且缺少 Math.max(left, right-view_w) 下限夹取；TS 已改为世界当前视宽 view_w(=dataset.screen_w)。分辨率/视宽可变时相机起点会与 TS 不一致。
  - TS：src/LFW/ui/component/GamePrepareLogic.ts:378-379（提交 070b579a2 引入 view_w，af1909373 定稿）
  - C++：native/lfw/ui/component/game_prepare_logic.cpp:627-629（screen_width() 定义于 :40）
  - 修复：改为 mt->range(left, std::max(left, right - world().dataset.screen_w()))，并删除固定 screen_width() 用法。
- [low] start_game 的出生带 xx1/xx2 仍按固定 794/3、2*794/3 计算，TS 已改为 view_w/3、2*view_w/3；非默认视宽时角色出生横向区间与地形 standable_x 的搜索范围会偏窄/偏宽。
  - TS：src/LFW/ui/component/GamePrepareLogic.ts:404-405（提交 070b579a2）
  - C++：native/lfw/ui/component/game_prepare_logic.cpp:659-660（screen_width() 定义于 :40）
  - 修复：用 world().dataset.screen_w() 替换 xx1/xx2 中的 screen_width()，保持与 cam_x 同一视宽来源。

> 备注：窗口提交：b25e6ff9c(10-18)…af1909373(11-13)，净 diff 49+/37-。已同步项：game_mode getter 与枚举化判定（on_resume:265、update:319、refresh_survival_rank:390、auto_start_when_ready:189、on_survival_rank_changed:222、start_game:575-576）；移除 lfw.survival_rank_mode / survival_rank_2p 并改设 lfw.game_mode（:265、:624）；reset_world_dataset 不再保留 difficulty（:542-554）；周期标签改用 set_i18n（:520，uinode.cpp:151 会立即取词条）；set_text 去掉 style 形参属 API 重构（C++ :89-92 等价）；lfw.string→lfw.t 在无插值参数时等价（LFW.ts:813）；world.ground→world.terrain 重命名为 ground.standable_x/segment/y，实现逐行等价（ground.cpp:79-89 对应 TerrainWorld.ts:302-312）。唯一缺口是 070b579a2 的视宽改造（cam_x 与 xx1/xx2），其余文件均无对应实现。

### src/DittoImpl/renderer/UINodeRenderer.ts — partial（9 次提交）

- [medium] 窗口内新增的输入框占位提示未移植：TS 把 props.placeholder 与 set_input_placeholder 写进 DOM input，C++ 的 TextInput/LineEdit 链路完全没有 placeholder 概念，联机房间列表的 network.pwd_hint 提示在 C++ 侧不显示。
  - TS：src/DittoImpl/renderer/UINodeRenderer.ts:142-147（dom.placeholder）、:187-189（set_input_placeholder），由窗口内 eb4fe49bf 新增；调用方 src/LFW/ui/component/network/RoomsLogic.ts:302-307（窗口内 aded27ecf，此前为 no-op）
  - C++：native/lfw/ui/component/text_input.cpp:16-25 只读 maxLength/defaultValue/fontSize/fontFamily；native/host/godot/lfw_godot_renderer.h:142-153 GodotTextInputBox 无 placeholder 字段；native/host/godot/lfw_godot_text_input.cpp:96-121 创建 LineEdit 从不断言 set_placeholder；native/lfw/ui/component/network_logic.cpp:653-657 绑定房间密码框只挂 on_submit，全程无 network.pwd_hint 键使用
  - 修复：给 ui::TextInput 增加 placeholder()（读 props.placeholder）与 GodotTextInputBox.placeholder 字段，在 GodotTextInputs::sync 里调用 LineEdit::set_placeholder，并在 network_logic 的房间行密码框上设置 t("network.pwd_hint")。
- [low] 输入框文本对齐只在节点自带文本非空时计算：C++ 把 style.text_align 的读取放在 if(!_text.empty()) 内，空输入的 TextInput（密码框典型）恒为左对齐，而 TS 对输入框无条件应用 node.style.text_align。
  - TS：src/DittoImpl/renderer/UINodeRenderer.ts:149（dom.style.textAlign = component.node.style.text_align \|\| 'left'，窗口内 c5cd3fa99 新增）
  - C++：native/host/godot/lfw_godot_ui_renderer.cpp:361-375（_text_align 只在 if (!_text.empty()) 分支内按 style.text_align 赋值，空文本保持 0）、:513 box.text_align = _text_align，:115-119 据此映射到 HORIZONTAL_ALIGNMENT_LEFT
  - 修复：把 font/fill_style/text_align 的样式读取移出 if (!_text.empty()) 分支，让空文本的输入框也能拿到节点 style 的对齐值。

> 备注：范围：git diff eb4fe49bf^..HEAD（窗口内 9 个提交，均 >=2026-10-01），native/ 被 .gitignore 忽略（.gitignore:69），C++ 只能按工作区文件审计；lfw/ui/ui_node_renderer.h 仅为接口（on_show/on_hide 等空默认），行为实现在 native/host/godot/lfw_godot_ui_renderer.cpp 与 lfw_godot_text_input.cpp。窗口内其余行为已同步：Esc→release_focus+on_cancel、Enter→set_text+on_submit 并拦截按键（lfw_godot_input.cpp:405-417，对应 TS preventDefault）、request_focus/request_blur、maxLength、fontSize/fontFamily 覆盖、bgAlpha/fgAlpha 默认 0（cpp:339/347）、style/language 变更重解析（sync_model 每 lifetime 重建，cpp:177-181）、image_desc 与 data.img 取值等价（cpp:270-286）、on_show/on_hide 语义由每帧 _visible=n.visible() 轮询覆盖（cpp:169）。另注：TS 新增的 input padding 0 6px/boxSizing 未在 Godot StyleBoxEmpty 上设内容边距，属纯 CSS 视觉差异，未单列 finding。

### src/LFW/Camera.ts — partial（8 次提交）

- [high] TS 纵向可见高度改用 world.view_h（=dataset.screen_h）决定 height<=view_h 提前退出与 cam_max_y；C++ 仍用常量 Defines.MODERN_SCREEN_HEIGHT，屏幕高与环境不等于该常量时，纵向退出条件与可移动上限均与 TS 不符。
  - TS：src/LFW/Camera.ts:92,103-105（提交 070b579a2、7993f1d56 引入 view_h=screen_h 并替换 MODERN_SCREEN_HEIGHT）
  - C++：native/lfw/camera.cpp:104（height <= modern_screen_height()）、115（view_h = modern_screen_height()/zoom_y）
  - 修复：把 view_h 改为 dataset.screen_h()，并让 camera.cpp:104 与 115 统一使用该 view_h。
- [medium] TS 已删除 view_h 的 zoom_y 除法，且 acc_y 分母同样用 view_h；C++ 的 view_h 仍除以 zoom_y，而 acc_y 分母却用 screen_h，zoom_y≠1 时纵向缓动与上限与 TS 不一致。
  - TS：src/LFW/Camera.ts:102-109（7993f1d56 删除 view_h=screen_h/zoom_y，109 行分母改 view_h）
  - C++：native/lfw/camera.cpp:113-115（zoom_y 除法保留）、123（分母 screen_h）
  - 修复：view_h 取 dataset.screen_h 不再除 zoom_y，acc_y 分母改用 view_h。
- [medium] TS 横向上限加了 max(min_cam_x,...) 保底并去掉 /zoom_x；C++ 仍是 cam_r - screen_w/zoom_x 且无 max 保底，地图窄于视口时上下界倒置，clamp 会把 destination.x/position.x 压到 cam_l 之外。
  - TS：src/LFW/Camera.ts:58-59（070b579a2 加 max 保底，7993f1d56 去掉 /zoom_x）
  - C++：native/lfw/camera.cpp:73-75
  - 修复：改为 max(min_cam_x, cam_r - dataset.screen_w())，保留 min_cam_x 兜底。

> 备注：窗口：34a403d25(11-09)..af1909373(11-13)，共 8 个提交。已同步项：cam_max_y 的 follow_y/cam_y_follow 分支（camera.cpp:116-119 对应 0d8f3b32e）；横向 acc_x 分母 screen_w 在 HEAD 等价于 view_w（World.ts:178 view_w=dataset.screen_w），无需改；lock/dest/reset/undest 生命周期与字段默认值一致。窗口内的 side=ui_left/zoom_x 偏移与 centered_cam_x 居中逻辑在 af1909373/5310a8606 中已回退，净变化为零，不作缺失。native 下未发现其它文件重复实现相机边界逻辑（camera.cpp 为唯一 update 实现）。

### src/LFW/cmds/CMD_F4.ts — partial（7 次提交）

- [high] 弹窗分支完全缺失。TS 在遍历每层时先看 layer.modal：有弹窗就关弹窗并 break，不再下探；C++ 完全没有 stock modal() 判断，直接走 pages 长度逻辑。
  - TS：src/LFW/cmds/CMD_F4.ts:14-18（commit fffe572be, 2026-11-13 新增 modal 块）
  - C++：native/lfw/cmds/cmd_f4.cpp:26-32（层循环内无 modal 判断）
  - 修复：在取 pages 长度前加：if (auto* m = layer->modal()) { if (!m->ignore_esc()) layer->close_modal(ui::UICloseModalOpts{}); break; }
- [high] 有弹窗时 C++ 的行为错两层：TS 只关最上层一个弹窗（on_modal_close 只收该弹窗）；C++ 会调 layer->pop()，而 pop() 先 close_modals() 关掉全部弹窗再弹页面，回调次数与页栈结果都不同。
  - TS：src/LFW/cmds/CMD_F4.ts:15 → src/LFW/ui/UILayer.ts:294-304（close_modal 无 id 只关栈顶一个）
  - C++：native/lfw/cmds/cmd_f4.cpp:31 → native/lfw/ui/uilayer.cpp:168（pop 开头 close_modals()）
  - 修复：弹窗路径必须单独走 UILayer::close_modal(单弹窗)，不要复用 pop()；同时补差分用例覆盖 open_modal 后 F4。
- [medium] modal.ignore_esc 语义缺失：TS 中顶层弹窗 ignore_esc 为真时仍 break，即 F4 整条命令什么都不做、绝不下探到更低层；C++ 无此提前终止，会继续向下找层并弹页面。
  - TS：src/LFW/cmds/CMD_F4.ts:16（modal 块内 break 无条件执行）
  - C++：native/lfw/cmds/cmd_f4.cpp:30-31（只有 layer.ui().ignore_esc 的 continue，无弹窗级阻断）
  - 修复：弹窗分支中无论 ignore_esc 真假都要 break，只在为假时才调用 close_modal。
- [low] 差分用例未覆盖弹窗场景，只测了 layer.pages 的 pop/不 pop，所以 C++ 缺 modal 分支没被测试发现。
  - TS：src/LFW/cmds/CMD_F4.ts:12-18
  - C++：native/tests/differential/cases/lfw/cmd_f4.txt:1-33（无 open_modal 用例）
  - 修复：新增用例：open_modal 后按 F4，断言 on_modal_close、只关一个弹窗、以及 ignore_esc 为真时无任何变化。

> 备注：窗口内共 7 个提交（24935298b..fffe572be）。其中两项行为改动已同步：`if (!length) continue`（b099810e1）对应 cmd_f4.cpp:28；`esc_through` 重命名为 `layer.ui.ignore_esc` 对应的跳过（a8601c43c）对应 cmd_f4.cpp:30。唯一缺失的是 fffe572be 新增的 modal 优先分支；C++ 移植最后一次更新在 2026-10-20（a2086e96b），早于该提交，UILayer::modal()/close_modal()/UINode::ignore_esc() 在 native/lfw/ui/uilayer.h:64,85 与 native/lfw/ui/uinode.h:204 均已具备，只差命令层接入。另：任务给的路径 lfw/cmds/cmd_f4.cpp 实际位于 native/lfw/cmds/。

### src/LFW/ditto/Instance.ts — partial（5 次提交）

- [low] IsDesktop 只有消费端没有取值端：C++ is_desktop() 仅基类默认 false，无任何宿主覆写，launch 页 desktop_hint 在原生桌面宿主下永不隐藏。
  - TS：src/LFW/ditto/Instance.ts:74-76（f808db10d 新增 IsDesktop）；使用处 src/LFW/ui/component/LaunchPageLogic.ts:156
  - C++：native/lfw/lfw.h:160 `virtual bool is_desktop() const { return false; }`（全仓仅 2 处引用，无 override）；native/lfw/ui/component/launch_page_logic.cpp:300-303 有隐藏逻辑但条件恒假
  - 修复：在 godot/thin 等桌面宿主 override is_desktop() 返回 true（web/wasm 宿主保持 false）。
- [low] print 无同名 C++ 成员，但语义由 lfw::log 承担，且 TS 唯一调用者是死代码 CMDS.print，无功能缺失，仅命名差异。
  - TS：src/LFW/ditto/Instance.ts:78（98421eb73 新增）；实现 src/init.ts:65（print: Log.print）；调用者 src/LFW/cmds/CMDS.ts:47-49（无 cmd handler 使用）
  - C++：native/lfw/lfw.h:182 `virtual void log(...)`；native/lfw/cmds/cmd_help.cpp:44 `ctx.world().lfw().log(...)`
  - 修复：无需修改；如需命名对齐可加 CMDS::print 转调 lfw.log。
- [low] 新增的 Networking/JSON5 服务字段在 C++ 均有对应实现（INetService/NetClient 与 core/json5），已同步，无需处理。
  - TS：src/LFW/ditto/Instance.ts:26-29,43,68（462aae87f/05b639850/c5cd3fa99）；消费处 src/LFW/PlayerInfo.ts:70、src/LFW/ui/component/network/network_logic
  - C++：native/lfw/net/net_service.h:80-120；native/host/net/net_client.h:23；native/lfw/lfw.h:307；native/lfw/core/json5.h；native/lfw/player_info.cpp:277
  - 修复：无需修改。

> 备注：窗口内 Instance.ts 共 5 个提交（f808db10d、462aae87f、98421eb73、05b639850、c5cd3fa99），diff 全部只增删 interface 成员/import type（IJSON5、JSON5、Networking、IsDesktop、print），运行时对象 `_Ditto` 与 vec2/vec3/setup 未变，因此该文件自身无运行时逻辑改动；核对的是这些新增服务字段所对应的运行行为。逐项结论：JSON5 与 Networking 在 C++ 已完整实现并接线；print 等价于 lfw.log；唯一实质差距是 IsDesktop 的取值端——C++ 移植了 launch 页的隐藏条件（launch_page_logic.cpp:300）但没有任何宿主把 is_desktop() 置真，导致该窗口内新增的"桌面壳隐藏 desktop_hint"行为在 C++ 永不生效。已只读审计，未修改任何文件（git status 仅原有未跟踪 debug.log）。

### src/LFW/bot/BotController.ts — synced（4 次提交）

- [low] C++ 残留 TS 已删除的死代码 target_dist_bound()（无调用者）与未使用局部变量 can_avoid；不影响运行时行为。
  - TS：4068c57cf（性能优化）已把 BotController::update_lookup 的距离边界换成 chase_bound()，BotController.ts:933-942 不再有 target_dist_bound
  - C++：native/lfw/bot/bot_controller.cpp:1152-1177（定义，无调用者）、:784（`const bool can_avoid = true;` 未使用）；native/lfw/bot/bot_controller.h:182（声明）
  - 修复：删除 target_dist_bound 的声明/定义以及 lookup() 里未使用的 can_avoid。

> 备注：窗口内 4 个提交（4a3df1faf, 4068c57cf, 68529b71f, b51be4294）对 BotController.ts 的净行为改动（git diff 4a3df1faf^..HEAD，243 行）已全部在 C++ 找到对应：(1) NEAR_SCAN_RADIUS=250 常量与 should_avoid/should_defend 的 box_gap 前置早退 —— bot_controller.h:184, cpp:693/746/1193，且 box_gap_xz 公式与 TS helper 完全一致；(2) NearestTargets 由 {entity,distance,defendable} 包装改为直接存 Entity*，look 去掉第三参、del 按指针身份 —— native/lfw/bot/nearest_targets.h/.cpp（box_gap_xz 参与 look/sort，id 作同距 tiebreak，与 TS 一致），defends.look 两参调用在 cpp:797-799/821/831 对应；TS 删掉的 defendable 在 src 全库已无使用，无行为损失；(3) update_lookup 改为 aabb_min/max_x 宽相位剪枝 + 动态 limit（初始 max(chase_bound,250)、chasings 数量变化时收缩） —— cpp:1199-1292 逐句对应（含 inf 分支、!l&&!r break、ghosted 判定、kept_prev 追踪）；(4) chase_bound 只看 chasings 且用 box_gap_xz —— cpp:1179-1191。68529b71f 与 b51be4294 互为反向（先删后恢复 watching 置空/置位逻辑与注释），净零；当前 TS 的 watching 逻辑与 cpp:561-565、636-639 一致。纯 TS 侧改动（en/av/atk_x getter 去掉 ?.entity、调试日志 v.id、…

### src/LFW/Ground.ts — partial（4 次提交）

- [medium] TS 删除 Ground.block() 并改写受阻推挤逻辑，C++ 仍保留 block() 并沿用旧四象限推挤。
  - TS：a52642fcc 删除 src/LFW/Ground.ts，block() 未迁入 src/LFW/terrain/TerrainWorld.ts（该文件无 block）；src/LFW/World.ts:584-587 改为按 prev_x/prev_z 退回 seg.x1-1 / seg.x2+1，World.ts:594 用 ret.y = y（不再抬到地形高度）
  - C++：native/lfw/ground.cpp:121-200 仍保留 block() 四象限推挤；native/lfw/world.cpp:808-824 仍调用 ground.block()，末尾 native/lfw/world.cpp:822 设置 restrict_result_.y = Ground::y(seg, x, z)
  - 修复：按 TS 重写 native/lfw/world.cpp restrict 的受阻分支：按上一帧所在侧退回并 clamp，y 保持原值，移除 ground.block() 调用。
- [low] Ground::y() 缺退化斜面零跨度保护，TS 返回 h1，C++ 除零得 NaN。
  - TS：src/LFW/terrain/TerrainWorld.ts:288 `if (seg.x1 === seg.x2) return seg.h1;` 与 :293 `if (seg.z1 === seg.z2) return seg.h1;`（a52642fcc 新增）
  - C++：native/lfw/ground.cpp:95-106 SlopeH/SlopeV 直接 (x - x1)/(x2 - x1)、(z - z1)/(z2 - z1) 除法，无零跨度判断
  - 修复：在 native/lfw/ground.cpp 的 SlopeH/SlopeV 分支除法前补 if (seg.x1 == seg.x2) return seg.h1; / if (seg.z1 == seg.z2) return seg.h1;
- [low] Ground::intersect() 缺斜面退化平面系数分支，零长度/零高差斜面在 TS 按水平面处理，C++ 仍用斜面系数。
  - TS：src/LFW/terrain/TerrainWorld.ts:374-378、384-388（a52642fcc 新增）dh===0\|\|len===0 时退化为 a=0,b=1,c=0,d=-h1
  - C++：native/lfw/ground.cpp:221-238 SlopeH/SlopeV 直接计算 a=-dh,b=dx2 / b=dz2,c=-dh，无退化分支
  - 修复：在 native/lfw/ground.cpp intersect 的 SlopeH/SlopeV 中补 dh==0 \|\| len==0 时按 Flat 取平面系数的分支。

> 备注：窗口内共 4 个提交：34a403d25、c222fc439、0687f793a、a52642fcc。核心 8 项行为改动已同步到 native/lfw/ground.cpp：segment 的 (one_way\|\|through)+y/rising/prev_y 过滤（ground.cpp:56-67，阈值 0.5 与 step=10 一致）、through+drop_through 分支（:60-62）、standable_x 步进 8..240（:79-89）、enterable 穿透时返回当前 y（:115）、block 对单向平台返回空（:128）、intersect 的 from_top 与跳过侧墙（:249、:268）、intersect_wall 跳过（:361）；ground.h:41-55 签名同步。数据与调用点亦就位：i_terrain_info.h:33-34 有 one_way/through，background_data.cpp:45-47 解析，item.cpp:179 与 game_prepare_logic.cpp:677 调 standable_x，world.cpp:780-782 传 drop_through。注意 src/LFW/Ground.ts 已在 a52642fcc 删除，逻辑迁至 src/LFW/terrain/TerrainWorld.ts，故后续以该文件为现行参照。范围外提示：调用方 drop_through 判定在 7a56d08a0 再次改写（World.ts:550-561：is_fighter && is_on_ground && terrain.through && (D\|\|Defend) && y > ground_y-1，并将 y 置为 ground_y-1），与 world.cpp:780-781 的 is_on_ground && !ctrl_is_end(Defend) && ctrl_ud() > 0（且不调整 y）不一致，应归入 World.ts 的审计项。

### src/LFW/ui/component/_.ts — partial（4 次提交）

- [high] TiledImage 未移植：C++ 既无该类也未注册，cls=TiledImage 的公共背景实例化失败，背景不再按设计尺寸重算平铺，直接退成 1x1。
  - TS：提交 3ac342788（2026-11-13「重构布局相关代码」）在 src/LFW/ui/component/_.ts:89 新增 _.TiledImage；TiledImage.ts:19-31 每帧按 node.w/design_w*repeat_x 写 img.repeatX/Y
  - C++：native/lfw/ui/component/component_registry.cpp:101-189 无 TiledImage 条目；native 下 grep TiledImage/tiled 零命中，无等价实现；数据 public/builtin_data/launch/bg.ui.json:13 用该 cls（design 794x450, repeat 20x11）；未注册时 native/lfw/factory.cpp:270 仅 warn 后 continue
  - 修复：新增 TiledImage 组件（repeat = node.w/design_w*repeat_x、node.h/design_h*repeat_y）并注册 "TiledImage"/"TileImage" 两个名字。
- [high] CoverImage 未移植：C++ 无 cover_scale，加载页的 CoverImage(target=root) 未注册被跳过，等待图不再按 max(sw/dw, sh/dh) 缩放覆盖。
  - TS：提交 1caa9235e（2026-11-13）在 _.ts:18 新增 _.CoverImage；CoverImage.ts:18-27 调 cover_scale 写 node.scale，算法见 cover_scale.ts:5-11
  - C++：component_registry.cpp:101-189 无 CoverImage；native 全仓无 cover_scale 及 design_w/design_h 处理；数据 lf2s/origin/prel_data/ui/loading_page.ui.json5:26、demo_loading_page.ui.json5:29、network_loading_page.ui.json5:26 均用 cls=CoverImage
  - 修复：实现 CoverImage（scale=max(target.w/design_w, target.h/design_h)，target=="root" 取 root）并在 registry 注册 "CoverImage"。
- [high] 联机三个 Logic 未按数据实际类名注册：C++ 只注册了 NetworkLogic，而数据写 NetworkModalLogic/RoomLogic/RoomsLogic，均被 warn 跳过，合并后的网络大厅逻辑无任何节点能拉起。
  - TS：提交 aded27ecf 在 _.ts:48,70,71 新增 _.NetworkModalLogic/_.RoomLogic/_.RoomsLogic（三者 TAGS 即这些名字，network/*.ts:17,24,30）
  - C++：component_registry.cpp:182 仅 regist_one<NetworkLogic>(u"NetworkLogic")，NetworkLogic::TAGS 也只有该名（network_logic.cpp:1427）；数据 network_modal.ui.xml:2、network_room_page.ui.xml:2、network_rooms_page.ui.xml:2 用的是另三个名字；全仓库无 cls="NetworkLogic" 引用，故 NetworkLogic 永不会被创建
  - 修复：在 component_registry.cpp 追加 regist_one<NetworkLogic>(u"NetworkModalLogic")、u"RoomLogic"、u"RoomsLogic" 三个别名（或把三名并入 NetworkLogic::TAGS）。

> 备注：窗口内其余改动已同步：ChatLogic（aded27ecf，_.ts:16）C++ 已注册（component_registry.cpp:183，数据 chat_page.ui.xml:2 可用）；Flex/FlexItem 的移除（1fa075ffb，_.ts 原 30-31 行）与 C++ 一致——native 无 Flex 类，改用 xml 节点 flex 属性（xml_to_ui_info.cpp:164 flex_decls_value），无需补；register 幂等守卫（TS _registed / C++ static registed，component_registry.cpp:102-104）亦一致。缺口集中在 5 个新增组件的注册与实现。只读审计，未修改任何文件。

### src/LFW/ui/component/MiscSettingsLogic.ts — missing（4 次提交）

- [high] 窗口新增的 render_resolution 行控件（ArrowSwitcher, row "render_resolution_row"）在 C++ 完全缺席，无 getter、无缓存字段。
  - TS：src/LFW/ui/component/MiscSettingsLogic.ts:32（提交 070b579a2）
  - C++：native/lfw/ui/component/misc_settings_logic.h:27-36 仅 10 个 getter；misc_settings_logic.cpp:94-104 DPtr 无对应成员，grep native/lfw 无 render_resolution 符号
  - 修复：在 h 中声明 ArrowSwitcher* render_resolution()，DPtr 增加 ArrowSwitcher* k，用 row_switcher(u"render_resolution_row") 缓存。
- [high] 分辨率行初始化与值改变回调未移植：set_items 填充预设标签，值变化写 render_w/h 并应用分辨率；C++ on_start 无此段，且 C++ ArrowSwitcher 没有 set_items。
  - TS：src/LFW/ui/component/MiscSettingsLogic.ts:58、108-112（提交 070b579a2/7993f1d56）
  - C++：native/lfw/ui/component/misc_settings_logic.cpp:226-345 on_start 结束无该逻辑；arrow_switcher.h:32-46 无 set_items（仅 slider_handle.h:44 有）
  - 修复：为 ArrowSwitcher 增加 set_items，并在 on_start 注册 render_resolution 的 on_value_changed（含 null 回退到预设 0）。
- [high] World.apply_render_resolution 与 RENDER_RESOLUTION_PRESETS / render_resolution_index / resolution_viewport 未移植，native 无 render_w/render_h 字段。
  - TS：src/LFW/defines/defines.ts:79-102；src/LFW/World.ts:168-176；MiscSettingsLogic.ts:110、134
  - C++：native/lfw 全库 grep 无 RENDER_RESOLUTION/render_w/render_h（仅 render_worker_id）；native/lfw/world.h:88-90 无 settings 成员
  - 修复：在 native defines 增加预设表与索引函数，World 增加 apply_render_resolution(w,h) 写 render_w/h 并按宽高比映射 screen_w/h。
- [medium] on_settings_change 回调缺 render_w/render_h 分支（应重新同步分辨率控件值）；C++ 仍监听 on_dataset_change 且只处理 sync_render，未随 TS 事件改名迁移。
  - TS：src/LFW/ui/component/MiscSettingsLogic.ts:43-46（提交 070b579a2、491809c12）
  - C++：native/lfw/ui/component/misc_settings_logic.cpp:234-243 仅 a.key==sync_render
  - 修复：改为监听 on_settings_change，键为 sync_render 或 render_w/render_h，后者调用 sync_render_resolution。
- [medium] on_resume 末尾未同步分辨率控件值；缺少 sync_render_resolution helper（按 render_resolution_index 取下标，越界回退 0）。
  - TS：src/LFW/ui/component/MiscSettingsLogic.ts:129、132-136（提交 070b579a2）
  - C++：native/lfw/ui/component/misc_settings_logic.cpp:347-371 on_resume 以 lang_row 结束，无该调用与 helper
  - 修复：新增 sync_render_resolution()，并在 on_resume 末尾调用，idx<0 时 set_value(0)。
- [medium] sync_render 存储目标未随 491809c12 迁移到 World.settings（客户端本地、非联机同步、可 dump），C++ 仍读写 WorldDataset；native 无 Settings 概念。
  - TS：src/LFW/ui/component/MiscSettingsLogic.ts:44、84、123、133；Settings.ts:12-38；World.ts:161-166
  - C++：native/lfw/ui/component/misc_settings_logic.cpp:302、363 用 dataset.set/sync_render；native/lfw/world.cpp:1193、1223 从 dataset 读取
  - 修复：引入 native Settings（含 sync_render/render_w/render_h 与变更回调），或显式确认沿用 dataset 并保持渲染重启语义。

> 备注：窗口内 4 个提交：0d8f3b32e 仅为类型断言（非行为）；070b579a2/7993f1d56 新增分辨率设置行及 World 分辨率 API；491809c12 将 sync_render/render_w/render_h 从 WorldDataset 迁到 World.settings（回调改名 on_settings_change）。C++ 侧完全没有任何分辨率相关实现（全库 grep 无 render_resolution/RENDER_RESOLUTION/render_w/render_h），也缺少 Settings 概念；sync_render 仍经 dataset 生效（world.cpp:1223 检测变化后 start_render），故音效/UPS/语言等旧行为已同步，但窗口内全部新行为未同步，判 missing。另：C++ ArrowSwitcher 无 set_items，是移植该行的前置缺口。

### src/LFW/bot/state/BotState_Chasing.ts — not-applicable（3 次提交）

- [low] 窗口内净改动仅为 chasings.get() 返回值由 IBotTarget 包装改为直接 Entity（移除 IBotTarget），属类型/结构重构，无运行时行为差异；C++ 侧本就以 Entity* 持有目标，无需同步新逻辑。
  - TS：src/LFW/bot/state/BotState_Chasing.ts:20（提交 4a3df1faf "设置页+prefix；移除IBotTarget"，2026-10-20；净 diff 4a3df1faf^..HEAD 仅此一行）
  - C++：native/lfw/bot/state/bot_state_chasing.cpp:44 `const Entity* const en = c.chasings.get();`；native/lfw/bot/nearest_targets.h:22 `const Entity* get() const;`
  - 修复：无需修复；如要保持三端一致，只需确认 native 的 chasings.get() 继续返回 Entity*（现状已满足）。

> 备注：窗口内（2026-10-01 起）该 TS 文件共 3 个提交：4a3df1faf（10-20，chasings.get() 去掉 ?.entity）、68529b71f 与 b51be4294（10-25，先删后加 is_leave_chase_range/is_leave_goto_range 与 absx>AI_STAY_CHASING_RANGE 块）。三者净 diff（4a3df1faf^..HEAD）仅剩第 20 行类型改动，因此窗口内无净行为性改动；且 git show 4a3df1faf^ 证实 is_leave_chase_range 与 AI_STAY_CHASING_RANGE 逻辑在 9-28 提交 fd025b07b 就已存在，属窗口外。即便如此，这两段逻辑在 C++ 中亦已实现：bot_state_chasing.cpp:45-46（is_leave_chase_range / is_leave_goto_range）、63-69（absx 与 BotBehavior::Stay/Follow → en_out_of_range 并转 Following），常量取 defines::num("Defines.AI_STAY_CHASING_RANGE")。update_running/update_dash/update_jump 的公式、阈值（400/100/50、-30/30、-60/60）与回调顺序均与 TS 一致。故判 not-applicable（窗口改动不涉及运行时行为）；不存在缺失项。

### src/LFW/ui/action/Actor.ts — partial（3 次提交）

- [medium] Actor.ts:23（c4e4ffb72, 2026-11-13）把 pop_page 动作的 min_pages 改为按所在层 index 取值：非 0 层为 0（允许弹空），0 层为 1（至少留一页）；C++ actor.cpp:48 仍固定 1.0，非 0 层（如 chat/overlay 层）只剩一页时 TS 能弹空、C++ 拒绝，行为不一致。
  - TS：src/LFW/ui/action/Actor.ts:22-24，关键行 23 `n.pop_page({ min_pages: n.layer?.index ? 0 : 1 })`；提交 c4e4ffb72（fix，2026-11-13）把此前的 `{ min_pages: 1 }`（8f6740320 引入）改为条件式。UILayer.index 见 src/LFW/ui/UILayer.ts:138，min_pages 语义见 UILayer.ts:231-234（保留 min_pages 页，默认 0）。
  - C++：native/lfw/ui/action/actor.cpp:46-50，关键行 48 `opts.min_pages = 1.0;`（不区分层）；UINode::pop_page 直接把 opts 传给 layer->pop（native/lfw/ui/uinode.cpp:1020），UILayer::pop 按 min_pages 截断（native/lfw/ui/uilayer.cpp:166-172），层 index 取值器见 native/lfw/ui/uilayer.h:55。
  - 修复：在 pop_page 处理器里按层号计算 min_pages：`UILayer* l = n.layer(); opts.min_pages = (l != nullptr && l->index() != 0.0) ? 0.0 : 1.0;`。

> 备注：窗口内（--since=2026-10-01）Actor.ts 共 3 个提交：8f6740320（11-03，内联处理器并删 set_ui/push_ui/pop_ui 别名，pop_page 由无参改为 min_pages:1）、0f017e4e3（11-03，新增 OpenModal/CloseModal 处理器）、c4e4ffb72（11-13，pop_page 改为按 layer.index 取 min_pages）。已核对：1) Modal 两个处理器 C++ 已有（actor.cpp:51-72），id 缺省/存在、去重（TS UILayer.ts:271-273、294-297 对 C++ uilayer.cpp:200-205、219-221）、回调顺序 on_modal_open/on_modal_close 均一致；2) 别名删除不构成移植缺口：同一提交 8f6740320 同时从 UIActionEnum.ts 删掉 SetUI/PushUI/PopUI，C++ 侧（ui_action_enum.h 仅有 set_page/push_page/pop_page）本来就没有旧名，且仓库 data/json 中无 "set_ui"/"push_ui"/"pop_ui" 用法；3) set_page/push_page 的 Number(index)\|\|0 与 C++ num_or_0 语义一致；4) 唯一未同步的是 c4e4ffb72 的层相关 min_pages。注意 native/tests/differential 的 TS 对照侧已退役（PROTOCOL.md 说明），故 cases/lfw/uilayer.txt:45-46 那条 layer2 的 actor pop_page 用例已无法自动暴露该差异。C++ actor.cpp 最后修改为 2026-10-20（a2086e96b），未跟进 11 月的 TS 改动。

### src/LFW/ui/component/DemoModeLogic.ts — partial（3 次提交）

- [medium] 视口宽度公式未同步：TS 现用 world.view_w（即 dataset.screen_w，已不再除背景 zoom），C++ 仍用 screen_w/zoom_x（窗口前的旧式），导致 VS 模式的 cam_x、min_x、max_x 计算在背景 zoom_x≠1 时不同，进而改变角色出生 X 与相机初始位置。
  - TS：src/LFW/ui/component/DemoModeLogic.ts:254 `const view_w = this.lfw.world.view_w`（7993f1d56、af1909373 收尾）；src/LFW/World.ts:178 view_w === dataset.screen_w（无 zoom 除法）。窗口起点（5e6e22ef8 之前）此处为 `dataset.screen_w / (bg.zoom_x \|\| 1)`。
  - C++：native/lfw/ui/component/demo_mode_logic.cpp:459-465 `const double zx = bg->zoom_x(); ... const double view_w = world().dataset.screen_w() / use_zx;` 并用于 cam_x/min_x/max_x（:463-465）。
  - 修复：把 demo_mode_logic.cpp:461 的 view_w 改为 world().dataset.screen_w()（或在 C++ World 上补 view_w 访问器后改用它），删除 zoom_x 除法，使 cam_x/min_x/max_x 与 TS 一致。

> 备注：窗口内共 3 个提交，行为性改动 2 处：(1) 5e6e22ef8「5s自动切demo」新增 _restart_delay_ms=5000 与 End 状态超时后 clearup()+startup() 自动重开——已同步（native/lfw/ui/component/demo_mode_logic.cpp:134 kRestartDelayMs=5000.0，:611-616 判 _fsm.state()==end_state 且 state_time>=5000 后 clearup/startup，顺序与 TS 一致；C++ 多一行 host().log 属日志，不计缺失）。(2) 7993f1d56/af1909373 把 demo 的 view_w 从 screen_w/zoom_x 改为 world.view_w(=screen_w)——未同步，见 finding。另注：同类旧式 zoom 除法在 native/lfw/world.cpp:926、native/lfw/ui/component/danmu_game_logic.cpp:374/841 也存在，说明「支持分辨率设置(070b579a2)」这轮改动整体可能未进 C++，建议提交者扩大审计范围；本报告仅按任务要求判定 demo_mode_logic。

### src/LFW/ui/component/TextInput.ts — partial（3 次提交）

- [medium] 窗口内新增的 placeholder 属性在 C++ 完全没有落地：无访问器、宿主也不给 LineEdit 设 placeholder，原生输入框提示文字会缺失。
  - TS：eb4fe49bf 在 src/LFW/ui/component/TextInput.ts:7,17 新增 placeholder 属性，DOM 渲染器 src/DittoImpl/renderer/UINodeRenderer.ts:142,147 将其下发为 dom.placeholder
  - C++：native/lfw/ui/component/text_input.h:22-25 只有 max_length/default_value/font_size/font_family；native/lfw/ui/component/text_input.cpp:16-26 无 placeholder()；native/host/godot/lfw_godot_renderer.h:142-153 GodotTextInputBox 无 placeholder 字段；native/host/godot/lfw_godot_text_input.cpp:94-121 只设字体/颜色/长度，从无 set_placeholder
  - 修复：在 text_input.h/.cpp 加 placeholder() 读 props_holder.str(u"placeholder")，给 GodotTextInputBox 加 placeholder 字段，并在 sync 中调用 LineEdit::set_placeholder（纳入 style_sig 以防每帧重设）。
- [low] c5cd3fa99 把 text 从成员改为 props 字段并声明 PROPS；C++ 仍只维护 _text，没有 props-backed text 也没有 props_meta，界面数据里给 text 属性会被丢弃。
  - TS：src/LFW/ui/component/TextInput.ts:13-22 声明 PROPS（text 为 nullable String），22 读取 props.text，24-28 写 props.text；渲染器 UINodeRenderer.ts:142,148 用 component.props.text 作为初始值
  - C++：native/lfw/ui/component/text_input.h:27-28,48-49 仅 _text 成员与 text()/set_text()；text_input.cpp 无 props u"text" 读取、无 PROPS()/props_meta()（对比 ui_component.cpp:155-162 的 ensure_props 校验链，其他组件如 char_menu_head.cpp:33-60 均声明 PROPS）
  - 修复：为 TextInput 补 PROPS()/<props_meta() 声明这 6 个属性，并在构造或首次 text() 时从 props_holder.str(u"text") 取初始值、set_text 时同步写回 props。
- [low] set_text 新增“同值早退 + 立即推给真实输入控件”的语义，C++ 仍无条件赋值；推送由宿主逐帧轮询兜住，但幂等早退未移植。
  - TS：src/LFW/ui/component/TextInput.ts:24-28（if (this.props.text == text) return; 后调用 renderer.set_input_value），set_input_value 见 src/DittoImpl/renderer/UINodeRenderer.ts:181-185
  - C++：native/lfw/ui/component/text_input.h:28 set_text 直接 _text = v，无相等判断；native/host/godot/lfw_godot_text_input.cpp:130-143 每帧比对 overlay->shown 与 input->text() 并把变化写回 LineEdit，替代了即时推送
  - 修复：在 set_text 中加 `if (_text == v) return;` 保持幂等语义（渲染推送已由宿主轮询覆盖，可不加回调）。

> 备注：窗口内 3 个提交（eb4fe49bf/d6933dc60/c5cd3fa99）中，on_submit、on_cancel、fontSize、fontFamily 均已同步：C++ text_input.h:22-25、31-32，宿主导入见 lfw_godot_text_input.cpp:94-121(字号/字体)、212-226(Enter/Esc 调 on_submit/on_cancel)，逻辑挂接见 chat_logic.cpp:145-146、network_logic.cpp:598-601,655,727。native/ 是独立 git 仓库（HEAD b11857c），text_input 最后一次改动为 f5bf5ee(2026-11-04)，此后无 C++ 侧补丁，故 Nov 5 的 text-in-props 改动尚未跟进。set_text 的即时推送在 C++ 由宿主逐帧轮询等价实现（lfw_godot_text_input.cpp:130-143），因此仅计低危。

### src/LFW/WorldDataset.ts — partial（3 次提交）

- [medium] 窗口新增两个字符串字段 weapon_rain_oids / weapon_rain_groups（默认空串）未进 C++ 默认值表：get 返回 undefined、tracked() 为 false、dump/XML 丢该键（reset_world_dataset 会写 undefined），与 TS 的 '' 默认不一致。
  - TS：src/LFW/WorldDataset.ts:108-109（提交 0d8f3b32e 新增）
  - C++：native/lfw/world_dataset.cpp:37-149 install_defaults 无这两个键；native/lfw/world_dataset.h:20-126 的字段宏只收 double；而 native/lfw/defines/fields_gen.h:231 的 world_dataset_fields 已含这两键，dump_dataset(native/lfw/world_dataset.cpp:194-205) 会按表取到 monostate
  - 修复：在 install_defaults 里以 Value(u"") 注册 weapon_rain_oids/weapon_rain_groups 并纳入 tracked()，使 get/dump/reset/callback 与 TS 对齐。
- [medium] 窗口内 TS 已把 sync_render 从 WorldDataset 删除并移入 Settings，C++ WorldDataset 仍保留该字段，world.cpp 仍从 dataset 读取渲染帧率，导致 dataset 字段集/dump 与 TS 源不一致（C++ 侧也没有 Settings 承接）。
  - TS：提交 491809c12「不需要同步的移动到Settings中」移除 sync_render；现由 src/LFW/Settings.ts:18 承载
  - C++：native/lfw/world_dataset.h:72；native/lfw/world_dataset.cpp:90 add(u"sync_render",3)；消费方 native/lfw/world.cpp:606、1193、1223 仍读 dataset.sync_render()
  - 修复：从 C++ WorldDataset 移除 sync_render，并仿照 Settings.ts 增加 settings 存储供 world.cpp 读取渲染帧率。

> 备注：窗口内 3 个提交：0d8f3b32e（新增 10 字段）、070b579a2（加 render_w/render_h）、491809c12（移除 render_w/render_h/sync_render）。render_w/render_h 窗口内先加后删，净变动为零，故不要求 C++ 同步。0d8f3b32e 新增的 8 个数值字段及默认值全部已同步：jump_hold_ctrl=1、cam_y_follow=0、air_ctrl=0、air_ctrl_accel=0、air_ctrl_accel_x/z=0、air_ctrl_max_speed_x=8、air_ctrl_max_speed_z=3（world_dataset.h:101-108、world_dataset.cpp:118-125），且运行时消费方已移植（camera.cpp:116、world.cpp:1107、entity.cpp:862-907、character_state_jump.cpp:112），Entity::dataset 回退到 world().dataset（entity.cpp:1101-1107）与 TS Entity.ts:2455-2462 顺序一致。注意 native/ 在 lfj 仓库未跟踪，git grep 查不到，需用文件系统搜索。另发现窗口外既有缺口（未计入）：land_friction_factor、toughness_r_tick 有默认值但缺具名访问器宏。

### src/LFW/cmds/CMD_ENTITY.ts — synced（2 次提交）

- [low] 报告数字格式化不一致：TS num() 四舍五入到 3 位小数且非有限值打印 '-', C++ num() 直接用 number_to_string 输出全精度, 非有限值打印 NaN/Infinity。velocity/lifetime 等未 round_float 的值会出现 0.333 vs 0.3333333333333333 的差异。
  - TS：src/LFW/cmds/CMD_ENTITY.ts:26-29 (num: Math.round(v*1000)/1000, 非有限→'-'), 用于 97-114 行报告
  - C++：native/lfw/cmds/cmd_entity.cpp:26 (num = number_to_string), 用于 90-114 行报告
  - 修复：把 C++ num() 改为先判非有限返回 u"-"，否则按 round(v*1000)/1000 后格式化。
- [low] --state 的取值校验语义不同：TS 先用 Number() 判 NaN 并报 "needs a number"，C++ 改用 value_of 再 States::has()，导致 --state=true 在 C++ 被当作 1 并设置状态(TS 报错)，而 --state= 空值在 C++ 报 "no such state code"(TS 按 Number('')=0 处理)。
  - TS：src/LFW/cmds/CMD_ENTITY.ts:154-160
  - C++：native/lfw/cmds/cmd_entity.cpp:157-164
  - 修复：先用 string_to_number(text) 判 NaN 并按 TS 文案告警，再走 state::entity_states().has()。

> 备注：窗口内只有两个提交：2e37d9056 (首次加入 CMD_ENTITY.ts，205 行全新) 与 db14bcf0f (仅把 stat_bar 加入 PROPS 及帮助文本)，因此按整份文件语义对比。核心行为均已同步：ENTITY 注册(cmd_entity.cpp / cmds.cpp:93)；find_entity_any 的 #前缀/ghosts/puppets 兜底(45-56)；value_of 的 true/false→1/0、null/none/空→空(59-65)；x/y/z 单轴保留其余(vs set_position 的 !nullish 分支, entity.cpp:1601-1604)；velocity 同时写 prev_velocity(140-141)；frame 的 special 分支与「数据里没有该帧则拒绝」等价于 TS 的 found.id!==text 校验(data_frame 为精确表查找, entity.h:319)；holding/bearer 走 set_prop→Entity::bind_hold，含旧反向引用拆除与对端互设，与 TS bind_hold 顺序一致(entity.cpp:1329-1366)；report 全部字段(data/name/team/position/velocity/frame 含 frame_state/hp-mp/facing/flags/other/link)与 KEY_WIDTH=10 对齐；未知选项走 set_prop 返回 false 再告警，19 个 PROPS 全部覆盖含 window 新增的 stat_bar(entity.cpp:1327)与帮助文本。仅存的差异是格式化与 value 强转边角(上述 2 条)，另 --name_visible= / =null 在 C++ 得 NaN(value_of 返回 monostate, to_number→NaN)而 TS 得 0(Number(null))，同属同类边角未单列。

### src/LFW/dat_translator/xml/xml_x_bg_layer.ts — partial（2 次提交）

- [high] xml_2_bg_layer 未解析 width_percent：TS 于 81a84a99a 新增该字段，并支持 w_percent 兜底（null 链），C++ 在 dh 之后直接跳到 uv_loop，读入 XML 时百分比宽度被丢弃。
  - TS：src/LFW/dat_translator/xml/xml_x_bg_layer.ts:24（提交 81a84a99a）
  - C++：native/lfw/dat_translator/xml/xml_x_bg_layer.cpp:44-45
  - 修复：在 dh 之后补 width_percent 解析，回退链 width_percent → w_percent → 原值，与 TS 行序一致。
- [high] xml_x_bg_layer 未写回 width_percent 属性：TS 在 dh 之后 set_attr，C++ 写出的 XML 丢失该字段，导致 C++ 端读取/回写往返后数据不一致。
  - TS：src/LFW/dat_translator/xml/xml_x_bg_layer.ts:54（提交 81a84a99a）
  - C++：native/lfw/dat_translator/xml/xml_x_bg_layer.cpp:74-75
  - 修复：在 set_attr(dh) 之后补 layer->set_attr(u"width_percent", field_or(l, u"width_percent"))。
- [medium] xml_2_bg_layer 未解析 center：TS 在 1f587a300 于 width_percent 之后新增该字段解析与默认值保留，C++ 解析缺该项，center 在 C++ 侧读取后丢失。
  - TS：src/LFW/dat_translator/xml/xml_x_bg_layer.ts:25（提交 1f587a300）
  - C++：native/lfw/dat_translator/xml/xml_x_bg_layer.cpp:45
  - 修复：在 width_percent 之后补 center 解析（get_num("center") 回退 bg_layer_info_new 默认值）。
- [medium] xml_x_bg_layer 未写回 center 属性：TS 在 width_percent 与 uv_loop 之间 set_attr，C++ 输出 XML 缺 center，破坏编辑器/运行时往返。
  - TS：src/LFW/dat_translator/xml/xml_x_bg_layer.ts:55（提交 1f587a300）
  - C++：native/lfw/dat_translator/xml/xml_x_bg_layer.cpp:75
  - 修复：在 uv_loop 之前补 set_attr(u"center", field_or(l, u"center"))。
- [low] 生成的定义表/schema 仍是旧版：bg_layer_info_fields 与 schema_i_bg_layer_info 均无 width_percent、center（TS IBgLayerInfo.ts 已有），补了解析后 reorder_fields 顺序会落在末尾，schema 也少两项。
  - TS：src/LFW/defines/IBgLayerInfo.ts:117-118,146-147（81a84a99a、1f587a300）
  - C++：native/lfw/defines/fields_gen.h:34; native/lfw/defines/schemas_gen.h:17
  - 修复：重跑 native/tools 下 gen_defines_fields.mjs、gen_defines_schemas.mjs（及 runtime）重新生成定义。
- [low] 该字段的数据模型与消费端也未同步：C++ BgLayerInfo 无 width_percent/center 访问器，bg/layer.cpp 宽度计算不含百分比逻辑（TS BgLayerRender.ts 用 width_percent 算图层宽度），仅补 XML 层仍无渲染效果。
  - TS：src/DittoImpl/renderer/BgLayerRender.ts:64-76
  - C++：native/lfw/data/bg_info.h:96-141; native/lfw/bg/layer.cpp
  - 修复：补 BgLayerInfo::width_percent()/center() 访问器，并在 bg 宽度计算中按 width_percent 缩放。

> 备注：窗口内仅两处行为改动：81a84a99a 增加 width_percent（含 w_percent 兜底）读写，1f587a300 增加 center 读写；C++ 两文件其余字段、pos/size/rect/offsetAnim 兜底链、z 取 index、delete_undefined→reorder_fields 顺序均一致。两项新字段在 native/ 全仓未实现（解析、写回、定义表、数据模型、渲染）。另注：C++ 的 z 直接用 index（TS 的 ?? ret.z 分支不可达，等价）。

### src/LFW/defines/defines.ts — missing（2 次提交）

- [high] RENDER_RESOLUTION_PRESETS 整表缺失：窗口内新增 1024×768、1600×1200、1280×800、1920×1200、2560×1080、3440×1440 六条，且所有固定尺寸项 label 改为 `W×H (a:b)` 并新增 aspect 字段。native 全仓无此标识，C++ 定义表也未收录。
  - TS：src/LFW/defines/defines.ts:81-95（070b579a2、7993f1d56）
  - C++：native/lfw/defines/runtime_gen.cpp:104-169（kRaw 无该条目）
  - 修复：重跑 native/tools/gen_defines_runtime.mjs 收录预设，并补上设置页与 world 侧消费。
- [high] viewport_from_aspect / resolution_viewport / parse_aspect 三个新函数缺失：逻辑视口按比例只向外扩、保证安全区完整可见（16:9→800×450、4:3→794×596、21:9→1050×450），且 w/h 为 0 或负时返回无效。C++ 逻辑视口恒为 794×450，无 aspect 概念，宿主也无 --aspect 等价参数。
  - TS：src/LFW/defines/defines.ts:44-66,100-103（070b579a2、7993f1d56）
  - C++：native/lfw/world_dataset.cpp:85-86; native/lfw/world.cpp:926-927
  - 修复：在 defines 模块实现这三个函数，并给 world 增加按比例改写 screen_w/screen_h 的入口。
- [medium] DEFAULT_RENDER_RESOLUTION 缺失：新增 aspect:"16:9"，label 由 "3176×1800" 变为 "3176×1800 (16:9)"；TS 的 Settings.render_w/render_h 默认值取自它，C++ 既无该常量也无这两个设置项。
  - TS：src/LFW/defines/defines.ts:79-80（7993f1d56）
  - C++：native/lfw/defines/runtime_gen.cpp:136-140（仅 DEFAULT_* 数值条目）
  - 修复：重跑生成器补 DEFAULT_RENDER_RESOLUTION，并在设置中补 render_w/render_h 及默认值。
- [medium] render_resolution_index(w,h) 缺失：按 w/h 精确匹配预设，不匹配返回 -1（设置页把 -1 落到 0）。C++ MiscSettingsLogic 没有 render_resolution_row，也没有该索引与当前分辨率的同步回调。
  - TS：src/LFW/defines/defines.ts:97-99; src/LFW/ui/component/MiscSettingsLogic.ts:132-135（070b579a2）
  - C++：native/lfw/ui/component/misc_settings_logic.cpp:299-305（只有 render_rate 行）
  - 修复：补 render_resolution_index，并实现设置页该行的取值/写回同步。
- [low] SAFE_SCREEN_WIDTH/SAFE_SCREEN_HEIGHT 缺失：MODERN_* 改为其别名（值仍 794/450，本身不改变行为），但语义上安全区与逻辑视口已分离；C++ 生成表只有 MODERN_*/CLASSIC_*，按安全区排版的代码直接取 dataset.screen_w。
  - TS：src/LFW/defines/defines.ts:35-38（070b579a2）
  - C++：native/lfw/defines/runtime_gen.cpp:120-121,139-140; native/lfw/ui/cook_ui_info.cpp:677-678
  - 修复：重跑生成器补 SAFE_SCREEN_*，并把按安全区尺寸排版处改用该常量。

> 备注：窗口内两个提交（070b579a2 支持分辨率设置、7993f1d56 未完成）对 defines.ts 全是新增：SAFE_SCREEN_* 及 MODERN_* 别名（值未变，老行为不受影响）、viewport_from_aspect/parse_aspect/resolution_viewport/render_resolution_index、DEFAULT_RENDER_RESOLUTION 加 aspect 与新 label 格式、预设表扩到 13 条。C++ 的 defines.cpp/h 本身只是数据表包装（数据由 native/tools/gen_defines_runtime.mjs 从 TS 的 Defines.* 数据属性生成），而生成物 runtime_gen.cpp 仍是窗口前旧版，68 条 Defines.* 里上述新条目一条都没有；函数不是数据属性、本就不进表，也没手写。消费端同样缺失：无 render_w/render_h、apply_render_resolution、render_resolution_row，逻辑视口固定 794×450。故窗口内行为性改动均未移植；纯重命名/类型/注释改动不计缺失。

### src/LFW/loader/preprocess_bg_data.ts — not-applicable（2 次提交）

- 无行为性缺失。

> 备注：窗口内该文件仅两次提交：b33e9be57(11-13) 给 layers 增加“最底层且无 absolute/loop 的层默认 width_percent=100、absolute=1”逻辑；5310a8606(11-13) 将其原样回退。git diff a42802965(08-31,窗口前基线) HEAD 对该文件输出为空，净行为变更为零，无需移植，C++ 现态亦无该逻辑，与 TS HEAD 一致。窗口前遗留(不在本次判定范围)：TS 会把 base.shadow 与每个 layer.file 作为 load_img 预载任务推入 jobs，native/lfw/loader/preprocess_bg_data.cpp 从不预载图片；dataset/layers 的 reorder_fields 在 C++ 中就地生效(共享 Object)，与 TS 等价。

### src/LFW/PlayerInfo.ts — synced（2 次提交）

- 无行为性缺失。

> 备注：窗口内仅 2 个提交，均非行为性。462aae87f 把 json5 包换成 Ditto.JSON5，而 src/DittoImpl/JSON5.ts:5 仍包装同一个 json5 包；c5cd3fa 把 data/blob 解码抽成 read_cache_string（src/LFW/ditto/cache/ICacheData.ts:65），逻辑与旧内联代码一致，仅 blob 读取异常的 warn 文案并入通用失败分支，属日志差异。C++ player_info.cpp:257-282 保留等价内联实现（data_kind/blob_kind + decode_utf8），json5_parse 语义对齐；字段、默认值、版本校验顺序、选中回调参数顺序均一致。附注：on_ctrl_changed 第三参 player 在 C++ 未传，为窗口前既有差异，不在本次范围。

### src/LFW/stage/Item.ts — synced（2 次提交）

- 无行为性缺失。

> 备注：窗口内仅 2 次提交：c222fc439 给 spawn 加地形落点(standable_x 取可站位 x、非指定 y 时取地形高，再 set_position(sx,sy,pz))；a52642fcc 只把 world.ground 改名 world.terrain。C++ item.cpp:178-184 已实现该落点；World 把背景 terrain 同时喂给 ground 与 terrain_world(world.cpp:183-184)，属结构差异，允许。生命周期/回调顺序/Times(0,120)/难度血量 3/4、3/2 公式/武器默认 y=300/is_soldier 分支均一致。细微差异：TS TerrainWorld.y 对退化坡面(x1==x2/z1==z2)返回 h1，C++ Ground::y 无此守卫，极端数据下 Item 生成点 y 可能 NaN（低）。

### src/LFW/state/CharacterState_Base.ts — partial（2 次提交）

- [medium] on_leave_ground 新增的 StateEnum.Defend 分支未同步：TS 中防御状态离地（如防御+下穿透地形）会进入 auto 帧转坠落，C++ 只匹配 Running/Walking/Standing/Rowing，Defend 离地后仍停在防御帧。
  - TS：src/LFW/state/CharacterState_Base.ts:53（commit 7a56d08a0）
  - C++：native/lfw/state/character_state_base.cpp:96-101
  - 修复：在 csb_on_leave_ground 的状态判断中加入 StateEnum::Defend。

> 备注：窗口内两处行为改动：c64ed22f3 把重武器离地时的 e.drop_holding() 改为直接 return，C++ 已同步（character_state_base.cpp:102 为 return，不丢弃手持物）；7a56d08a0 的 void velocity 是 TS 类型噪声，C++ 的 (void)velocity 等价。其余字段/默认值/公式/回调顺序一致（landing_2、heavy_obj_walk、default、in_the_skys[0]、falling[1][1]、falling[-1][1]、cvx_d/cvy_d、NEXT_FRAME_AUTO vs frame_id::kAuto 均对应）。注意该提交主体是 World.ts/TerrainWorld.ts 的地形穿透逻辑，不在本文件审计范围。

### src/LFW/ui/component/CameraCtrl.ts — missing（2 次提交）

- [high] 自动跟随镜头的目标坐标公式未同步。窗口内把 cam_x = staring.x - screen_w/(2*zoom_x)、cam_y = -round(z)/2 - screen_h/(2*zoom_y) 改为 world.view_w/view_h 的一半（view_w 就是 dataset.screen_w，不再除 zoom）。C++ 仍按旧公式除以 zoom_x/zoom_y，zoom≠1 时相机目标偏差可达 (1-1/zoom)*view/2，被注视目标明显偏离画面中心，纵向同样偏。
  - TS：src/LFW/ui/component/CameraCtrl.ts:57-62（提交 7993f1d56 + af1909373；World.ts:178-179 view_w=dataset.screen_w）
  - C++：native/lfw/ui/component/camera_ctrl.cpp:90-104
  - 修复：改为 cam_x = pos.x - screen_w/2、cam_y = -0.5*round(pos.z) - screen_h/2，删除 zoom_x/zoom_y 除法。
- [low] 注视目标高亮描边缺默认色。TS 的 staring setter 在赋新目标时若 outline_color 为空则设为 'rgb(255, 0, 0)'；C++ set_staring 只清理旧目标（宽度 1、颜色空），不设红描边，选中敌人无红边。（该差异早于本次窗口，非窗口内回归，但至今未移植。）
  - TS：src/LFW/ui/component/CameraCtrl.ts:20-30
  - C++：native/lfw/ui/component/camera_ctrl.cpp:46-53
  - 修复：在 C++ set_staring 赋新目标后，若 outline_color 为空则 set_outline_color(u"rgb(255, 0, 0)")。

> 备注：窗口内（2026-10-01 起）该 TS 只有两次行为改动，均在 update 的自动跟随块：7993f1d56 把 screen_w/(2*zoom) 换成 design_view_w/2，af1909373 再改为 view_w/2（=dataset.screen_w，不再含 zoom）。C++ 移植提交 9f23d1cf6（2026-10-11）停在 2f99bfd6c 的 zoom 版公式，故窗口改动完全未同步。native 侧尚无分辨率/viewport 重构（无 render_w、无 view_w 访问器，world.cpp:926、entity.cpp:1248 仍在本地用 screen_w/zoom 折算），camera.cpp 也未同步 TS Camera.ts 的 view_w 化，故本项修复需与 world 的 view_w 语义一起落地。其余逻辑（j/d/LR-UD 键分支、5*dt 平移、outline_width 呼吸、hp<=0/mounted 时 focus_lr(1)、on_stop 复位、focus_lr/focus_ud 排序取模、candidates 仅在 focus_lr 生效）均已对齐。

### src/LFW/ui/component/DanmuGameLogic.ts — partial（2 次提交）

- [medium] 窗口内两次提交把可见宽度公式从 dataset.screen_w/(bg.zoom_x\|\|1) 改为 world.view_w（即 dataset.screen_w，不再除 zoom）。C++ 两处仍按 screen_w/zoom_x 计算 view_w，zoom_x≠1 时镜头 x、DIST_CAM 值以及 update_bg 的左右夹取边界都与 TS 不同。
  - TS：src/LFW/ui/component/DanmuGameLogic.ts:173,501（af1909373 改为 world.view_w；7993f1d56 曾改为 design_view_w）
  - C++：native/lfw/ui/component/danmu_game_logic.cpp:374-375,841-842
  - 修复：用 const double view_w = world().dataset.screen_w(); 去掉 zoom_x 除法，与 World.view_w 对齐。

> 备注：窗口内 DanmuGameLogic.ts 只有这两处同源改动，其余常量、加入队列/清理、cheer CD 与回血比例、coop 模板/重生、幸存者收集、观众统计排序、回调注册顺序在 C++ 中均有对应实现，语义一致。附带说：native world.cpp:926、camera_ctrl.cpp:100 仍保留 zoom 除法，属同一轮分辨率改造未同步，非本文件范畴。

### src/LFW/ui/component/LaunchPageLogic.ts — synced（2 次提交）

- 无行为性缺失。

> 备注：窗口内 2 个提交均无未同步行为。f808db10d：window.runtime 改为 Ditto.IsDesktop，ditto/Instance.ts:63 仍为 !!window.runtime，C++ host().is_desktop() 等价，Godot 宿主 lfw_godot_setup.cpp:703 隐藏 desktop_hint。8152622ff：on_resume 的 find_child 改为 search_node，因布局重构把 bearface 等节点嵌入 launch_center>launch_content；C++ 属性绑定 ui_props.cpp:89-96 已用 node.search_node 递归解析，同样命中嵌套节点。FSM/动画/音频/回调均一致。仅 desktop_hint 仍用 find_child，但它是根的直接子节点，行为等价。

### src/LFW/helper/UIHelper.ts — not-applicable（1 次提交）

- 无行为性缺失。

> 备注：窗口内（2026-10-01 起）该文件仅有一次提交 6f8583787，diff 仅为 import 路径从 ../ui/ICookedUIInfo 改为 ../loader/ui/ICookedUIInfo（ICookedUIInfo 迁移到 loader），属 type-only 导入路径调整，无行为语义变化，故无可同步项。C++ 侧 UIHelper 现有实现与 TS 保持一致：_all 容器 + all() 只读视图、clear() 清空并返回自身、add() 追加到末尾并返回自身、push_page/set_page 构造 {id} 对象后带上 stack_idx 调用 LFW 对应方法（默认 0）。另注（非窗口内改动，不计入 findings）：TS add() 会为核心 UI 动态注册 push_<id>/switch_<id> 便捷方法，C++ 仅有 push_page/set_page 且需显式传 id；该差异先于本窗口存在，未在本次审计范围内。

### src/LFW/loader/preprocess_bdy.ts — synced（1 次提交）

- 无行为性缺失。

> 备注：窗口内仅 fa82fca70 一次提交，唯一行为改动是 actions 由 forEach 原地写回改为 map 返回新数组。因 preprocess_action 原地改写 action 且返回同一对象（preprocess_action.ts:9-31），两版数据完全一致，C++ 原地循环（preprocess_bdy.cpp:111-120）等价。其余均对应：Normal+Caught→AllBoth（cpp:68-72）、1000..1999→Criminal+双组 CondMaker+ensure 两个 action（cpp:74-106）、__tester（TS 为 Expression，C++ 存 test 源串，由 WorldCollisionHost::tester_run 用同一 collision getter 懒编译并缓存，语义等价）、prefab 合并与错误（cpp:58-63）、id 减 1000（cpp:103）。

### src/LFW/loader/preprocess_itr.ts — synced（1 次提交）

- 无行为性缺失。

> 备注：窗口内本文件仅 1 次提交 fa82fca70(2026-11-10)，改动仅一处：itr.actions?.forEach((n,i,l)=>l[i]=preprocess_action(...)) 改为 if (itr.actions) itr.actions = itr.actions.map(n=>preprocess_action(...))。TS 的 preprocess_action 原地改字段并返回同一 action 对象，因此 map 与 forEach 的元素效果、顺序完全相同，唯一差别是 itr.actions 换成新数组实例。C++ preprocess_itr.cpp:322-331 按相同顺序对每个 actions 元素原地调用 preprocess_action（Value 基于 shared_ptr，元素对象共享，字段变更对数组元素可见），行为等价；数组身份差异对下游不可观察，且 prefab 路径下 C++ resolve_prefab.cpp:106 已深拷贝 actions。故窗口内行为性改动已同步，窗口外既有差异不在本次判定范围。

### src/LFW/ui/chat_overlay.ts — synced（1 次提交）

- 无行为性缺失。

> 备注：窗口内该文件只有一个提交 aded27ecf（2026-11-06）新建全部 62 行，均为行为逻辑。C++ 侧完整对应：CHAT_LAYER=2；is_chat_open 按 page id 匹配；open/peek 均先判已开、先置 peek 再 push；close 用空 opts 弹顶层页；toggle 在 peek 时先关再开（回到输入态）。peek 状态落在 ChatStore::peek（默认 false，ChatLogic::on_stop 复位），与 TS 模块级 peeking 等价，仅命名/结构不同。TS 的 set_chat_escape_handler/handle_chat_escape 在 C++ 由 ChatHotkey 的 Esc 分支（预览态关闭）与 TextInput::on_cancel → ChatLogic::exit_input（输入态退出输入）分层实现，语义一致。未发现缺失。

### src/LFW/ui/component/ExtraZipText.ts — synced（1 次提交）

- 无行为性缺失。

> 备注：窗口内仅 1 次提交 cec7c610c：lfw.string 改为 lfw.t。该提交把 LFW.string 与 string_with_args 合并为 t(name,...args)，t 无参等价旧 string；本文件两处调用均无参，属纯重命名，无行为变化。C++ extra_zip_text.cpp:43/45 用 LFW::string，其实现 lfw.cpp:698-700 与旧 string 一致；i18n 缺失回退返回 key 名，i18n.cpp:151-161 与 I18N.ts:82-88 一致。故空串判断、拼接、空串写空格、start 注册双回调后 render、stop 退订均一致；differential cases texts.txt:76-87 已覆盖。

### src/LFW/ui/component/FighterHead.ts — synced（1 次提交）

- 无行为性缺失。

> 备注：窗口内唯一提交 a952d19dc 仅调整 import 顺序并把 player_id 取值从 v.data.values 改为 v.values；该版本 UINode 构造时已把 data.values 复制到 this.values（HEAD 由 UINodeBuilder.fill 写入），二者内容相同，属纯重构，无行为变化。C++ 侧 player_id() 以同样方式从父节点 values.player_id 上溯取值（UINode::data() 中的 cooked info 携带 values），args[0] 优先、空值回退、120 尺寸、RFACE 回退与 head 字段读取均与 TS 语义一致，未发现缺口。

### src/LFW/ui/component/GameModeFSMState.ts — partial（1 次提交）

- [high] 窗口内唯一行为改动：GameModeFSMState_End 的 enter/leave 查 score_board 由 find_child 改为 search_node（递归查找）。C++ 仍用 find_child，只查直接子节点。现在 score_board 位于 页面/__center/__design 下（3 层），find_child 恒返回空，结算时分数板不会显示/隐藏。
  - TS：src/LFW/ui/component/GameModeFSMState.ts:41,45（提交 1caa9235e 2026-11-13：find_child → search_node，两处）
  - C++：native/lfw/ui/component/game_mode_fsm_state.cpp:38,43 仍为 node.find_child(u"score_board")
  - 修复：把 game_mode_fsm_state.cpp 两处 find_child 改成 node.search_node(u"score_board")（uinode.cpp:516 已实现同语义递归查找）。

> 备注：窗口内（>=2026-10-01）该 TS 只有 1 个提交 1caa9235e，行为差仅 score_board 查找方式（find_child 只查直接子节点，search_node 递归；uinode.ts:922-933 与 native uinode.cpp:502-524 语义一致，端口已有 search_node 且其它组件普遍用它）。布局证据：vs_mode_page.ui.xml:19 中 score_board 位于 vs_mode_page/__center/__design 下，VsModeLogic 挂在页根；stage_mode_page.ui.json5:142、demo_mode_page.ui.json5:130 同样深 3 层。native 为该 TS 的独立仓库，game_mode_fsm_state.* 最后一次改动为 2026-10-20 初始导入 d3913d7，早于 TS 提交。文件其余语义（Running 的 paused/game_result、BeforeEnd 的 _time 清零与 3000ms 阈值、End 的 win→pass 否则 end 音效、key 默认值）两侧一致，无其它缺失。

### src/LFW/ui/component/MoveTable/MoveTableLogic.ts — synced（1 次提交）

- 无行为性缺失。

> 备注：窗口内唯一改动 cec7c610c：5 处 lfw.string() 改为 lfw.t()。该提交把 string()+string_with_args() 合并为 t(name,...args)，无参数时 t() 仅返回 _i18n.string(name)，而本文件调用点全为单参数，故属纯 API 重命名、无行为变化。C++ 对应 5 处（now_loading、keys、name、desc、move_table.mp）用 lfw().string(Value)，语义相同，无需移植 %N 替换。其余常量、公式、阶段机、end_delay 回退、_entity_fit 缩放在 C++ 均已一致。

### src/LFW/ui/component/PlayerCtrlType.ts — synced（1 次提交）

- 无行为性缺失。

> 备注：窗口唯一改动 a952d19dc：player_id 父查找由 v.data.values 改为 v.values（values 转 public）。等价：提交时 UINode 构造已把 data.values 复制到 this.values，HEAD 由 UINodeBuilder.fill 以 Object.assign(node.values, info.values) 复制 cook 结果，构建后无写入；C++ player_ctrl_type.cpp:16-28 经 data()/info().values() 读同一份，属允许结构差异。args[0] 优先、父链上溯、命中取 player_id 语义一致。轮换(+5/+1 mod 6)、触屏互斥、子节点显隐、resume 注册并刷新、pause 解绑与 C++:61-105 一致；仅容错不同（C++ 空 player 早返回）。

### src/LFW/ui/component/PlayerKeyText.ts — synced（1 次提交）

- 无行为性缺失。

> 备注：窗口内唯一改动 a952d19dc：父节点查找谓词由 v.data.values 改为 v.values（UINode.values 由 protected 转 public）。语义等价性成立：node.values 仅由 UINodeBuilder.fill 用 Object.assign(node.values, info.values) 从 cook 产物 info.values 复制，全仓库无构建后对 node.values 的写入；C++ player_key_text.cpp 通过 v.data()/CookedUIInfo::values() 读取同一份 info.values，属允许的结构差异。其余逻辑（args[0]/args[1] 回退、SHORT_KEY_CODES+to_upper_case、空值 None、on_resume 注册回调并立即刷新、on_pause 解绑）均与 TS 一致，无行为缺口。

### src/LFW/ui/component/PlayerNameInput.ts — partial（1 次提交）

- [medium] c5cd3fa99 删除了 maxLength getter（原 props_holder.num('maxLength') ?? 10），TS 侧 PlayerNameInput 无 props 时不再有 10 字上限（DOM renderer 无 maxLength 即 removeAttribute）；C++ 仍保留 max_length() 返回 value_or(10.0)，godot 宿主据此把 LineEdit 限到 10 字，设置界面里的 PlayerNameInput() 仍只能输入 10 字。
  - TS：提交 c5cd3fa99 删除 'get maxLength() { ... ?? 10 }'；当前文件 src/LFW/ui/component/PlayerNameInput.ts:1-26 已无 maxLength override
  - C++：native/lfw/ui/component/player_name_input.cpp:22-24（消费方 native/host/godot/lfw_godot_text_input.cpp:94,120）
  - 修复：删除 PlayerNameInput::max_length() override（回落 TextInput 的 props.maxLength，不带 10 默认值）。

> 备注：窗口内仅 c5cd3fa99 一次改动，含 2 处行为变更。on_name_changed 由直接写 _text 改为 set_text：C++ TextInput::set_text 仅是 _text=v，宿主 sync（lfw_godot_text_input.cpp:130-142）会把 _text 回写 LineEdit，故等价。defaultValue override 删除无行为影响：TS 基类 TextInput 仍返回 props.defaultValue，C++ 保留的 override 与基类实现相同，且全仓库无 default_value() 消费者。max_length() 仅 godot 宿主消费。start/stop/resume/blur 与回调顺序一致，无其他缺口。

### src/LFW/ui/component/PlayerScoreCell.ts — synced（1 次提交）

- 无行为性缺失。

> 备注：窗口内唯一提交 983c65773 仅改 update()：node.text = new TextInfo({text,style}) 改为 node.set_text(txt)，语义为解除 i18n 绑定、文本未变时复用已测量文本、并按 style.version 重新自适应尺寸。C++ player_score_cell.cpp:41 已调用 node.set_text(txt)；native uinode.cpp:132 的 set_text 同样清 _i18n_key、同文本复用并 auto_size_by_text（text_size_key 含 style.version），与 TS 一致。kind/get_txt 的 kill、attack、hp_lost、mp_usage、picking、status 分支、null->"-"、_last_txt 默认无值、_last_style_version=-1、hp>0/lose/win_dead 优先级均与 TS 逐条对应；style.h 的 set_field 值未变化不 bump version，与 TS Style 一致，故缓存门控等价。

### src/LFW/ui/component/ReachableGroup.ts — partial（1 次提交）

- [medium] 窗口内唯一行为性改动：binded_layout 由 node.root.find_child(lid) 改为 node.root.search_node(lid)，即从“仅查直接子节点”变为“深度优先递归查找后代”。C++ 仍用 find_child，仅能命中直接子节点；当 args[2] 指向更深层后代时 C++ 找不到而回退到自身 node，导致 on_key_down 的 visible/disabled 门控作用于错误节点（可能错误放行或屏蔽方向键）。
  - TS：ReachableGroup.ts:16（提交 1caa9235e 重构布局相关代码，2026-11-13）；UINode.ts:922 find_child 仅遍历 children，UINode.ts:926-933 search_node 递归子孙
  - C++：lfw/ui/component/reachable_group.cpp:32-37（第35行 find_child）；native/lfw/ui/uinode.cpp:516-524 已有等价 search_node 可复用
  - 修复：binded_layout() 中把 node.root().find_child(lid) 改为 node.root().search_node(lid)。

> 备注：窗口内（自 2026-10-01）仅 1 个提交触及该 TS 文件，diff 只有 find_child→search_node 一行。逐项核对：group/direction 取 args[0]/args[1] 且缺省空串，C++ str(0)/str(1) 等价；on_start 按 group_name 收集 Reachable 并按 lr/ud 的 global_pos 排序，C++ 一致；focus_prev 的 max(idx,0)+len-1 取模、focus_next 的 (idx+1)%len、idx=-1 时落到首/末项，均一致；on_key_down 的 lr/ud switch 穿透与按键分发、visible/disabled 早退、focused=true 的 set_focused 语义（含 on_blur/on_focus 回调）一致；update 覆写仅转调 super.update（C++ ui_component.h:101 为空实现），无行为差异。现有 entry_page.ui.xml 只写 args="main,ud"，未触发第三参数路径，故实际影响有限，但逻辑确未同步。

### src/LFW/ui/component/ScrollView.ts — partial（1 次提交）

- [medium] 指针 scene→UI 坐标换算未同步：TS 改用 world.ui_from_scene（按运行时视口 screen_w/screen_h 换算），C++ 仍用固定 MODERN_SCREEN 常量。视口尺寸不等于 794×450 时，内容拖拽、滚轮滚动、滚动条轨道/thumb 命中判定会与宿主（UiInputHandle 用 dataset 视口尺寸做命中）不一致，偏移量与差值成正比。
  - TS：src/LFW/ui/component/ScrollView.ts:439-454（提交 070b579a2，2026-11-13）
  - C++：native/lfw/ui/component/scroll_view.cpp:455-478，常量助手在 :25-26
  - 修复：在 pointer_axis_px/pointer_in_view/pointer_in_node 内改用 world().dataset.screen_w()/screen_h() 换算（等价 host::ui_point_of_scene），删除 screen_width()/screen_height() 常量助手。

> 备注：窗口内 ScrollView.ts 只有一次提交 070b579a2（支持分辨率设置），行为差异仅此一处 scene→UI 坐标换算；其余 diff 为 import 清理。文件其余逻辑（字段/默认值/40px 步长/0.4 平滑/8px 阈值/KEY_REPEAT_MS=80/裁剪/follow_focus/滚动条双通道同步/回调顺序）与 C++ 一一对应。默认视口 794×450 时两种换算结果相同，故 native 当前不可观测；且 native 尚未移植分辨率设置（无 render_w/viewport_from_aspect，dataset screen_w 恒为 794），此差异为潜在缺陷。注意 host/common/ui_input_handle.cpp:42 已有等价 ui_point_of_scene，说明该提交其余部分已移植，仅 UI 组件内命中换算遗漏；slider_handle.cpp:193-200 同样遗留旧写法（非本次审计对象）。

### src/LFW/ui/component/Slider/ArrowSwitcher.ts — missing（1 次提交）

- [medium] 窗口内唯一行为性改动是为 ArrowSwitcher 新增运行时改 items 的 API：`set items` 赋值器 + set_items(v: readonly string[])。它把数组 join 成逗号串写回 props.items，把 _value 钳制到 max(0, len-1)，并立即 sync_label()。C++ 只有只读 items()，没有 set_items/写 props 的入口，运行时无法追加或替换选项；且 C++ 的 _value 不会被写入钳制（仅 value() 读取时 min），选项先缩小再扩大后 TS 停在钳制后的下标、C++ 会恢复旧下标，标签也会在下次同步前保持旧值。
  - TS：src/LFW/ui/component/Slider/ArrowSwitcher.ts:68-77（提交 7993f1d56 2026-11-13，窗口内唯一改动）
  - C++：native/lfw/ui/component/arrow_switcher.h:40-52（无 set_items）；native/lfw/ui/component/arrow_switcher.cpp:73-83, 124-134
  - 修复：在 ArrowSwitcher 增加 set_items(const std::vector<std::u16string>&)：逗号 join 后 props_holder.set_str(u"items")，将 _value 钳制到 max(0, n-1) 再调用 sync_label()。

> 备注：窗口（since 2026-10-01）内该 TS 文件仅 1 个提交 7993f1d56，diff 仅 +10 行，即 items setter/set_items，其余逻辑均在窗口前已实现。逐项比对确认已同步：默认值 _value=0/_hl=0、◀/▶ 缺省字形、responser 回退到自身节点、hover 高亮 k=min(1,dt/90) 平滑、press 先置焦点再 step、step 的 wrap 取模与 n<=1 提前返回、set_value 的 round+钳制且不触发回调、on_value_changed 回调参数 (value, this)、同步时文本比较避免重复 set_text、高亮叠层与箭头同字形。C++ 无 set_items 的同族实现，native 下仅 SliderHandle::set_items 存在，故非“实现在别处”。补充：该新 API 在 TS 的唯一调用点 MiscSettingsLogic.ts:58 的 render_resolution 尚未移植到 native（native 无 resolution 相关标识），因此当前 C++ 无实际调用者，但 API 与钳制/重同步语义缺失仍是实质差异。

### src/LFW/ui/component/Slider/SliderHandle.ts — missing（1 次提交）

- [high] 070b579a2 把滑块点击/拖动坐标由常量 MODERN_SCREEN_WIDTH/HEIGHT 改为 world.ui_from_scene，按逻辑视口 dataset.screen_w/h 映射；C++ handle_pointing_event 仍用常量 794/450，宽高比非默认时滑块与 UI 坐标系错位、拖动写入错误值。
  - TS：src/LFW/ui/component/Slider/SliderHandle.ts:146-161（提交 070b579a2）；World.ts:178-185 ui_from_scene=(sx+1)/2*view_w
  - C++：native/lfw/ui/component/slider_handle.cpp:193,200（screen_width()/screen_height() 常量）
  - 修复：在 C++ World 增加按 dataset.screen_w/h 的 ui_from_scene，并在 handle_pointing_event 用它替换常量换算。

> 备注：窗口内仅 1 个提交 070b579a2，该文件行为改动只有坐标换算一处（移除 Defines 导入非行为改动）；C++ 移植（2026-10-11）早于它。native/ 无 ui_from_scene 实现；dataset.screen_w/h 存在（默认 794/450），cook_ui_info.cpp:675-680 已按 dataset 尺寸建 UI 根，故滑块用常量会与 UI 坐标系不一致。同模式遗留另见 scroll_view.cpp:457-476（未计本报告）。其余语义一致：字段默认值、switcher 初始化、on_p_down 特判与 grab、三个指针回调顺序、尺寸公式、update 的 0.25 缓动与键盘越界回绕。

### src/LFW/ui/component/StageTitleShow.ts — synced（1 次提交）

- 无行为性缺失。

> 备注：窗口内唯一改动为 cec7c610c（2026-11-06）：on_stage_change 中 this.lfw.string(title 源) 改为 this.lfw.t(title 源)。同一提交在 LFW.ts 把 string(name) 重命名为 t(name,...args) 并合并原来的 string_with_args（t = _i18n.string 后做 %1..%n 替换）。本调用点不传参，故 t 与旧 string 行为完全一致，纯 API 重命名。C++ 侧 native/lfw/ui/component/stage_title_show.cpp:77 用 lfw().string(t)（lfw.h:399 → lfw.cpp:698 → I18N::string，默认语言、别名回退、未命中回退 key 名），与 TS t 的零参数语义等价；title→bg.name→"" 的 nullish 回退、顺序、opacity 序列（0→1/500、Delay1500、1→0/500）、stage/world 回调注册与注销均对应。无行为性缺失。

### src/LFW/ui/component/UIProps.ts — synced（1 次提交）

- 无行为性缺失。

> 备注：窗口内唯一改动是 6f85837 把 UIProps.ts:39 的 parse_ui_value 作用域由 owner.node.data 改为 owner.node（IUIValueScope 解耦），算法/默认值/回退均未变。C++ ui_props.cpp:62 仍传 cooked data，但 C++ UINode 无独立运行时 values 存储（uinode.cpp:557 读 info().values()），cooked 的 values/template_values 与 parent 链（cook_ui_info.cpp:328/740）跟运行时节点链一一对应，且全仓无运行时改写 node.values 或重挂父节点，故 $val: 语义等价。若将来支持运行时 values 写入，需给 UINode 加访问器并改传节点。

### src/LFW/ui/read_info_value.ts — not-applicable（1 次提交）

- 无行为性缺失。

> 备注：窗口内仅一次提交 6f8583787：参数类型由 ICookedUIInfo 抽象为 IUIValueScope（values/template_values/parent）并补注释，函数实现体零改动，属 type-only，无行为语义变化。新增 Tests/ui_value_scope.test.ts 断言的行为（自身值优先父链、values 优先 template_values、缺失返回 null）C++ 已覆盖（read_info_value.cpp:55-70、84-92）。另注（先于本窗口，不计入 findings）：Clazz 分支 C++ 恒返回 null（:127-129）；C++ 先 trim 再取前缀后 5 字符，TS 用未 trim 原串 substring(5)，前导空白时结果不同。

## 附录：无同名 C++ 实现的 TS 文件（需人工判断是否移植）

共 168 个（已排除接口/纯类型/测试用例）。改动次数前 40：

| TS 文件 | 提交数 |
| --- | --- |
| src/DittoImpl/renderer/BgLayerRender.ts | 11 |
| src/LFW/ditto/networking/INetworking.ts | 9 |
| src/LFW/cmds/index.ts | 8 |
| src/LFW/ui/IUIInfo.dat.ts | 7 |
| src/LFW/ui/ICookedUIInfo.ts | 7 |
| src/LFW/ui/index.ts | 6 |
| src/LFW/ui/UINodeBuilder.ts | 5 |
| src/DittoImpl/renderer/CursorRender.ts | 5 |
| src/DittoImpl/renderer/WorldRenderer.ts | 5 |
| src/LFW/ui/component/index.ts | 4 |
| src/LFW/defines/IWorldDataset.ts | 4 |
| src/LFW/ui/component/network/RoomLogic.ts | 4 |
| src/DittoImpl/ui/HeadlessUIInputHandle.ts | 4 |
| src/LFW/defines/IBgLayerInfo.ts | 3 |
| src/LFW/ui/ui_helpers.ts | 3 |
| src/LFW/index.ts | 3 |
| src/LFW/dat_translator/make_bg_data.ts | 3 |
| src/LFW/loader/ui/ICookedUIInfo.ts | 3 |
| src/LFW/defines/index.ts | 3 |
| src/DittoImpl/sounds/Modern.ts | 3 |
| src/DittoImpl/renderer/UITextRenderer.ts | 3 |
| src/LFW/cmds/CMD_KEY_EVENTS.ts | 2 |
| src/DittoImpl/ImageMgr/ImageMgr.ts | 2 |
| src/LFW/ui/component/network/RoomsLogic.ts | 2 |
| src/LFW/entity/release_point.ts | 2 |
| src/LFW/ditto/render/IWorldRenderer.ts | 2 |
| src/LFW/defines/IDirectorData.ts | 2 |
| src/LFW/ui/component/TiledImage.ts | 2 |
| src/DittoImpl/renderer/HeadlessUINodeRenderer.ts | 2 |
| src/DittoImpl/Keyboard.ts | 2 |
| src/LFW/ui/component/network/NetworkModalLogic.ts | 2 |
| src/LFW/dat_translator/fighters/make_fighter_data_firzen.ts | 2 |
| src/LFW/ui/IUINodeInit.ts | 2 |
| src/LFW/dat_translator/bots/make_bot_data_firzen.ts | 2 |
| src/LFW/ui/component/Tests/Firen/Firen_DVJ.ts | 1 |
| src/LFW/ui/IUIAction.ts | 1 |
| src/LFW/ui/component/Tests/Firen/index.ts | 1 |
| src/LFW/ui/component/Tests/Firezen/Firzen_DUA.ts | 1 |
| src/LFW/ui/component/Tests/Firezen/Firzen_FUSION.ts | 1 |
| src/LFW/ui/component/Tests/Firezen/index.ts | 1 |

