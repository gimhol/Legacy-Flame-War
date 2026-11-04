#pragma once

#include <string>

namespace lfw::ui {

namespace ui_action {
inline const std::u16string kAlert = u"alert";
inline const std::u16string kLinkTo = u"link_to";
inline const std::u16string kSetPage = u"set_page";
inline const std::u16string kPushPage = u"push_page";
inline const std::u16string kPopPage = u"pop_page";
/** 在当前页面上打开弹窗：`open_modal(ui_id)` */
inline const std::u16string kOpenModal = u"open_modal";
/** 关闭当前层弹窗：`close_modal()` / `close_modal(ui_id)` */
inline const std::u16string kCloseModal = u"close_modal";
inline const std::u16string kLoopImg = u"loop_img";
inline const std::u16string kLoadData = u"load_data";
inline const std::u16string kBroadcast = u"broadcast";
inline const std::u16string kSound = u"sound";
inline const std::u16string kSwitchDifficulty = u"switch_difficulty";
inline const std::u16string kDestoryStage = u"destory_stage";
inline const std::u16string kRemoveAllEntities = u"remove_all_entities";
inline const std::u16string kExit = u"exit";
}

}
