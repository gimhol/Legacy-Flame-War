#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "lfw/base/no_emit_callbacks.h"
#include "lfw/core/value.h"
#include "lfw/helper/join_queue.h"
#include "lfw/stage/stage.h"
#include "lfw/ui/component/camera_ctrl.h"
#include "lfw/ui/component/summary_logic.h"
#include "lfw/utils/times.h"

namespace lfw {

class Entity;

namespace ui {

class DanmuGameLogic : public SummaryLogic {
 public:
  static constexpr const char* TAG = "DanmuGameLogic";
  static constexpr const char16_t* BROADCAST_ON_START = u"DanmuGameLogic_ON_START";
  static constexpr const char16_t* BROADCAST_ON_STOP = u"DanmuGameLogic_ON_STOP";
  static constexpr double MAX_FIGHTERS = 32;
  static constexpr double COOP_TEMPLATES = 4;
  static constexpr double TEAMS8_TEAM_CAP = 4;
  static constexpr double STAGE_SWITCH_DELAY = 60 * 3;
  static constexpr double FFA_SEEDS = 8;
  static constexpr double CHEER_CD = 60 * 3;
  static constexpr double CHEER_HP_RATIO = 0.15;
  static constexpr double QUEUE_IDLE_TIMEOUT = 60 * 300;

  struct ViewerStat {
    std::u16string uid;
    std::u16string name;
    double spawns = 0;
    double kills = 0;
    double deads = 0;
    double damages = 0;
    double cheers = 0;
    bool alive = false;
  };

  struct CheerEvent {
    std::u16string uid;
    std::u16string name;
    double time = 0;
  };

  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  DanmuGameLogic(UINode& layout, const std::u16string& f_name, const Value& info);
  ~DanmuGameLogic() override;

  const ClazzTag* clazz() const override { return class_tag(); }
  const std::u16string& props_tag() const override;
  const Value& props_meta() const override;

  const std::u16string& mode();
  const std::vector<CheerEvent>& cheer_feed() const;
  std::u16string stage_name();
  bool waiting_next_stage() const;

  double time = 0;

  void update_teams();
  void on_fighter_add(Entity& e) override;
  void on_fighter_del(Entity& e) override;
  void on_start() override;
  void on_stop() override;
  void update(double dt) override;

  void update_bg();
  bool join(const helper::Entrant& entrant);
  bool leave(const std::u16string& uid);
  void touch(const std::u16string& uid);
  bool enter(const helper::Entrant& entrant);
  bool switch_to(const std::u16string& uid, const Value& oid = Value());
  bool cheer(const std::u16string& uid);
  std::vector<ViewerStat> viewer_stats();
  void update_staring();

  helper::JoinQueue join_queue{50};

  const std::vector<std::pair<std::u16string, double>>& team_caps() const;
  const std::map<std::u16string, ViewerStat>& viewers() const;
  CameraCtrl* cam_ctrl() const;
  bool coop_stage_ready() const;
  bool waiting_respawn() const;
  const std::set<std::u16string>& teams_set() const;
  double staring_countdown() const;
  double gameover_countdown() const;
  double next_stage_delay() const;
  double queue_sweep_timer() const;
  const std::map<std::u16string, double>& cheer_cd() const;
  const std::map<std::u16string, double>& queue_seen() const;
  const std::optional<std::vector<helper::Entrant>>& survivors() const;
  const std::optional<std::u16string>& join_fallen() const;

 protected:
  void sweep_queue();
  void try_join();
  bool claim_queued();
  const std::vector<Value>& regular_fighter_datas();
  std::optional<Value> template_data();
  Entity* find_claimable_template();
  bool claim_template(const helper::Entrant& entrant);
  void place_coop_spawn(Entity& e);
  void fill_templates();
  std::optional<Value> regular_data(const Value& oid);
  Entity* spawn_entrant(const helper::Entrant& entrant, const std::u16string& team,
                        const Value& data = Value());
  void track_viewer(Entity& e);
  void untrack_viewer(Entity& e);
  Entity* find_viewer_fighter(const std::u16string& uid);
  bool start_coop_stage();
  std::vector<helper::Entrant> collect_survivors();
  void respawn_viewers();
  std::vector<Entity*> staring_candidates();
  std::optional<std::u16string> find_join_team();
  void fighter_enter(Entity& v);

 private:
  struct DPtr;

  const std::unique_ptr<DPtr> dptr;
};

}
}
