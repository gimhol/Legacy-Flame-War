# 无同名 C++ 文件的 TS 模块定性（2026-10-01 以来有改动）

口径：路径/内容启发式 + 在 `native/**/*.{h,cpp}` 里检索该文件导出的标识符。

| 分类 | 数量 |
| --- | --- |
| gap | 6 |
| partial-elsewhere | 6 |
| unknown-runtime | 3 |
| ported-elsewhere | 44 |
| type-only | 20 |
| re-export | 17 |
| web-host | 18 |

## 需要人工确认（gap / partial / unknown）

| TS 文件 | 分类 | 导出标识符 | native 中缺失 |
| --- | --- | --- | --- |
| src/LFW/cmds/CMD_F5.ts | gap | TURBO_PLAYRATE | TURBO_PLAYRATE |
| src/LFW/defines/ISettings.ts | gap | settings_fields | settings_fields |
| src/LFW/ui/IUIAction.ts | gap | ui_action_clone | ui_action_clone |
| src/LFW/ui/UINodeBuilder.ts | gap | UINodeBuilder | UINodeBuilder |
| src/LFW/utils/array/array_del.ts | gap | array_del | array_del |
| src/LFW/utils/type_check/instance_of.ts | gap | instance_of | instance_of |
| src/LFW/defines/IOpointInfo.ts | partial-elsewhere | ALL_OPOINT_KIND,ALL_OPOINT_SPREADING,ALL_OPOINT_MULTI_ENUM,opoint_info_new,opoint_info_fields | ALL_OPOINT_KIND,ALL_OPOINT_SPREADING,ALL_OPOINT_MULTI_ENUM |
| src/LFW/loader/get_val_getter_from_stage.ts | partial-elsewhere | stage_world_val_getters,stage_val_getters,get_val_getter_from_stage | stage_world_val_getters |
| src/LFW/ui/ui_helpers.ts | partial-elsewhere | measure_text_width,fit_text,paint | measure_text_width,fit_text |
| src/LFW/ui/utils/check_field.ts | partial-elsewhere | one_of,arr_of,non_nagative_int,check_field | check_field |
| src/LFW/utils/type_check/is_bool.ts | partial-elsewhere | is_bool,is_false,is_true | is_false |
| src/LFW/utils/type_check/is_num.ts | partial-elsewhere | is_num,is_f_num,is_zero,is_one,is_nan,is_finite,is_nagtive,is_positive,not_zero_num,num_or,is_int,is_not_zero_int,is_positive_int,is_non_nagative_int,is_non_positive_int,is_nagative_int,is_num_arr,is_undefined_or_number | is_f_num,is_zero,is_one,is_finite,is_nagtive,is_not_zero_int,is_non_positive_int,is_nagative_int |
| src/LFW/cmds/CMD_DATASET.ts | unknown-runtime | - | - |
| src/LFW/cmds/CMD_KILL_SOLDIERS.ts | unknown-runtime | - | - |
| src/LFW/cmds/CMD_SET_LANG.ts | unknown-runtime | - | - |

## 其余分类

| TS 文件 | 分类 |
| --- | --- |
| src/LFW/collision/handle_ball_frozen.ts | ported-elsewhere |
| src/LFW/dat_translator/bots/make_bot_data_firzen.ts | ported-elsewhere |
| src/LFW/dat_translator/copy_itr_info.ts | ported-elsewhere |
| src/LFW/dat_translator/fighters/make_fighter_data_firzen.ts | ported-elsewhere |
| src/LFW/dat_translator/fighters/make_fighter_data_julian.ts | ported-elsewhere |
| src/LFW/dat_translator/fighters/make_fighter_data_rudolf.ts | ported-elsewhere |
| src/LFW/dat_translator/make_bg_data.ts | ported-elsewhere |
| src/LFW/defines/IArmorInfo.ts | ported-elsewhere |
| src/LFW/defines/IBdyInfo.ts | ported-elsewhere |
| src/LFW/defines/IBgData.ts | ported-elsewhere |
| src/LFW/defines/IBgLayerInfo.ts | ported-elsewhere |
| src/LFW/defines/IBotDataSet.ts | ported-elsewhere |
| src/LFW/defines/IBpointInfo.ts | ported-elsewhere |
| src/LFW/defines/ICpoint.ts | ported-elsewhere |
| src/LFW/defines/IEntityData.ts | ported-elsewhere |
| src/LFW/defines/IEntityInfo.ts | ported-elsewhere |
| src/LFW/defines/IFrameInfo.ts | ported-elsewhere |
| src/LFW/defines/IFramePic.ts | ported-elsewhere |
| src/LFW/defines/IItrInfo.ts | ported-elsewhere |
| src/LFW/defines/INextFrame.ts | ported-elsewhere |
| src/LFW/defines/IPictureInfo.ts | ported-elsewhere |
| src/LFW/defines/IPurePlayerInfo.ts | ported-elsewhere |
| src/LFW/defines/IStageInfo.ts | ported-elsewhere |
| src/LFW/defines/IStageObjectInfo.ts | ported-elsewhere |
| src/LFW/defines/IStagePhaseInfo.ts | ported-elsewhere |
| src/LFW/defines/IWorldDataset.ts | ported-elsewhere |
| src/LFW/defines/IWpointInfo.ts | ported-elsewhere |
| src/LFW/defines/Platform.ts | ported-elsewhere |
| src/LFW/Settings.ts | ported-elsewhere |
| src/LFW/state/CharacterState_Standing.ts | ported-elsewhere |
| src/LFW/state/CharacterState_Teleport2FarthestAlly.ts | ported-elsewhere |
| src/LFW/state/CharacterState_Teleport2NearestEnemy.ts | ported-elsewhere |
| src/LFW/ui/component/CharMenu/CharMenuLogic.ts | ported-elsewhere |
| src/LFW/ui/component/CoverImage.ts | ported-elsewhere |
| src/LFW/ui/component/network/default_nicknames.ts | ported-elsewhere |
| src/LFW/ui/component/network/NetworkModalLogic.ts | ported-elsewhere |
| src/LFW/ui/component/network/RoomLogic.ts | ported-elsewhere |
| src/LFW/ui/component/network/RoomsLogic.ts | ported-elsewhere |
| src/LFW/ui/component/TiledImage.ts | ported-elsewhere |
| src/LFW/ui/cover_scale.ts | ported-elsewhere |
| src/LFW/utils/string_parser/take_sections.ts | ported-elsewhere |
| src/LFW/utils/type_cast/to_num.ts | ported-elsewhere |
| src/LFW/utils/type_check/is_arr.ts | ported-elsewhere |
| src/LFW/utils/type_check/is_str.ts | ported-elsewhere |
| src/LFW/cmds/CMD_KEY_EVENTS.ts | type-only |
| src/LFW/defines/IDataLists.ts | type-only |
| src/LFW/defines/IDirectorData.ts | type-only |
| src/LFW/defines/IDirectorIndex.ts | type-only |
| src/LFW/ditto/cache/ICacheData.ts | type-only |
| src/LFW/ditto/image/ITextInfo.ts | type-only |
| src/LFW/ditto/importer/IImporter.ts | type-only |
| src/LFW/ditto/networking/IClientInfo.ts | type-only |
| src/LFW/ditto/networking/INetworking.ts | type-only |
| src/LFW/ditto/networking/IRoomInfo.ts | type-only |
| src/LFW/ditto/pointings/IPointings.ts | type-only |
| src/LFW/ditto/render/IUINodeRenderer.ts | type-only |
| src/LFW/ditto/render/IWorldRenderer.ts | type-only |
| src/LFW/entity/IEntityCallbacks.ts | type-only |
| src/LFW/ILFWCallback.ts | type-only |
| src/LFW/IWorldCallbacks.ts | type-only |
| src/LFW/loader/IEntityDataContext.ts | type-only |
| src/LFW/loader/ui/ICookedUIInfo.ts | type-only |
| src/LFW/types/globals.d.ts | type-only |
| src/LFW/ui/IUIInfo.dat.ts | type-only |
| src/DittoImpl/index.ts | re-export |
| src/LFW/bot/index.ts | re-export |
| src/LFW/cmds/index.ts | re-export |
| src/LFW/dat_translator/xml/index.ts | re-export |
| src/LFW/defines/index.ts | re-export |
| src/LFW/director/index.ts | re-export |
| src/LFW/ditto/index.ts | re-export |
| src/LFW/ditto/networking/index.ts | re-export |
| src/LFW/index.ts | re-export |
| src/LFW/loader/index.ts | re-export |
| src/LFW/loader/ui/index.ts | re-export |
| src/LFW/terrain/index.ts | re-export |
| src/LFW/ui/component/index.ts | re-export |
| src/LFW/ui/component/network/index.ts | re-export |
| src/LFW/ui/component/Tests/index.ts | re-export |
| src/LFW/ui/index.ts | re-export |
| src/LFW/utils/index.ts | re-export |
| src/DittoImpl/__PointingEvent.ts | web-host |
| src/DittoImpl/ImageMgr/ImageMgr.ts | web-host |
| src/DittoImpl/Importer.ts | web-host |
| src/DittoImpl/Keyboard.ts | web-host |
| src/DittoImpl/Pointings.ts | web-host |
| src/DittoImpl/renderer/BgLayerRender.ts | web-host |
| src/DittoImpl/renderer/CursorRender.ts | web-host |
| src/DittoImpl/renderer/EntityMainRender.ts | web-host |
| src/DittoImpl/renderer/HeadlessUINodeRenderer.ts | web-host |
| src/DittoImpl/renderer/TerrainIndicator.ts | web-host |
| src/DittoImpl/renderer/UITextRenderer.ts | web-host |
| src/DittoImpl/renderer/WorldRenderer.ts | web-host |
| src/DittoImpl/sounds/Modern.ts | web-host |
| src/DittoImpl/ui/HeadlessUIInputHandle.ts | web-host |
| src/LFW/ditto/image/TextInfo.ts | web-host |
| src/LFW/ditto/importer/BaseImporter.ts | web-host |
| src/LFW/ditto/networking/nickname.ts | web-host |
| src/LFW/ditto/networking/recommend_sync.ts | web-host |

## 人工确认结论（对上面 15 个可疑项）

| TS 文件 | 结论 |
| --- | --- |
| `src/LFW/cmds/CMD_F5.ts` | **已移植**：`lfw/cmds/cmd_f.cpp` 有 `cmd_f5` 与 `kTurboPlayrate = 10.0`，文案一致（导出常量名不同导致误报） |
| `src/LFW/cmds/CMD_DATASET.ts` / `CMD_KILL_SOLDIERS.ts` / `CMD_SET_LANG.ts` | **已移植**：`cmd_dataset` / `cmd_kill_soldiers` / `cmd_set_lang` 均在 `cmds.cpp` 注册（这三文件只有副作用注册、无导出，所以被归到 unknown-runtime） |
| `src/LFW/defines/ISettings.ts` | **等价实现**：C++ 用 `WorldSettings{sync_render, render_w, render_h}` + `World::set_sync_render/apply_render_resolution`，没有 `settings_fields` 的 dump 表；目前没有调用方需要 dump |
| `src/LFW/ui/IUIAction.ts` | **不需要**：`ui_action_clone` 是值拷贝 helper，C++ 的 `UIAction` 是值类型 |
| `src/LFW/ui/UINodeBuilder.ts` | **概念已移植**：C++ 由 `UINode::create` + `cook_ui_info` 承担 |
| `src/LFW/utils/array/array_del.ts`、`utils/type_check/instance_of.ts` | **不需要**：C++ 无对应调用者（数组删除用 `erase`，类型判断用 ClazzTag/`is_class`） |
| `src/LFW/defines/IOpointInfo.ts` 的 `ALL_OPOINT_*` | **不需要**：只给编辑器/JSON schema 的 options 用，运行期不用；枚举表在 `defines/opoint_kind.h` 等已生成 |
| `src/LFW/loader/get_val_getter_from_stage.ts` | **等价实现**：`World::stage_value` / `bot_controller.stage_value` 提供同语义取值 |
| `src/LFW/ui/ui_helpers.ts` | `measure_text_width` 由宿主 `measure_text` 等价；**`fit_text` 未移植**（联机玩家行标签不会按 150px 截断，纯视觉，low） |
| `src/LFW/ui/utils/check_field.ts` | **等价实现**：C++ 走 schema 校验（`validate_fields`/`schemas_gen.h`） |
| `src/LFW/utils/type_check/is_bool.ts` / `is_num.ts` | **部分等价**：C++ `utils/type_check.h` 覆盖了运行期用到的判断；`is_false/is_zero/is_finite` 等未用到的小谓词没有逐字对应（low） |
