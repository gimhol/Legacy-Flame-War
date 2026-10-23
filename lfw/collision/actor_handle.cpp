#include "lfw/collision/actor_handle.h"

#include "lfw/buff/buff_group_attack.h"
#include "lfw/controller/base_controller.h"
#include "lfw/entity/entity.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/type_cast.h"

namespace lfw {
namespace collision {
namespace {

Value from_str(const std::u16string* p) { return p == nullptr ? Value() : Value(*p); }

}

ActorHandle ActorHandle::of(const Entity& e) {
  ActorHandle h;
  h._entity = &e;
  h._id = e.id;
  return h;
}

ActorHandle ActorHandle::of(const CollisionActor& a) {
  ActorHandle h;
  h._id = a.id;
  h._snap = a;
  h._snap_built = true;
  return h;
}

const std::u16string& ActorHandle::id() const {
  if (_entity != nullptr) return _id;
  ensure_snapshot();
  return _snap.id;
}

const Entity* ActorHandle::entity() const {
  if (_entity == nullptr) return nullptr;
  return _entity->id == _id ? _entity : nullptr;
}

void ActorHandle::ensure_snapshot() const {
  if (_snap_built) return;
  _snap_built = true;
  const Entity* const e = entity();
  if (e == nullptr) {
    _snap = CollisionActor();
    _snap.id = _id;
    return;
  }
  CollisionActor& a = _snap;
  a = CollisionActor();
  a.id = e->id;
  a.px = e->position.x;
  a.py = e->position.y;
  a.pz = e->position.z;
  a.data_id = e->data()->id();
  a.data_type = e->data()->type();
  a.frame = e->frame;
  a.itr_prefabs = e->data()->itr_prefabs();
  if (e->bearer != nullptr) a.bear_wpoint_attacking = e->bearer->frame->wpoint().attacking();
  a.marks_group_attack =
      e->marks.find(std::u16string(buff::Buff_GroupAttack::KIND)) != e->marks.end();
  a.dropping = e->dropping;
  a.arest = e->arest();
  a.has_catcher = e->catcher != nullptr;
  a.catcher_hurtable =
      e->catcher == nullptr ? Value() : Value(e->catcher->frame->cpoint().hurtable());
  a.invulnerable = e->invulnerable();
  a.bot_ignore = e->bot_ignore();
  a.team = e->team();
  const std::u16string* const em = e->emitter();
  if (em != nullptr) a.emitter = *em;
  a.spawn_time = e->spawn_time();
  a.is_bot_ctrl = e->ctrl() != nullptr && e->ctrl()->is_bot();
}

const CollisionActor& ActorHandle::snapshot() const {
  ensure_snapshot();
  return _snap;
}

CollisionActor& ActorHandle::snap_mut() {
  ensure_snapshot();
  _entity = nullptr;
  return _snap;
}

double ActorHandle::px() const {
  const Entity* const e = entity();
  return e != nullptr ? e->position.x : snapshot().px;
}

double ActorHandle::py() const {
  const Entity* const e = entity();
  return e != nullptr ? e->position.y : snapshot().py;
}

double ActorHandle::pz() const {
  const Entity* const e = entity();
  return e != nullptr ? e->position.z : snapshot().pz;
}

std::u16string ActorHandle::data_id() const {
  const Entity* const e = entity();
  return e != nullptr ? e->data()->id() : snapshot().data_id;
}

double ActorHandle::data_type() const {
  const Entity* const e = entity();
  return e != nullptr ? e->data()->type() : snapshot().data_type;
}

const FrameInfo* ActorHandle::frame() const {
  const Entity* const e = entity();
  const FrameInfo* const f = e != nullptr ? e->frame : snapshot().frame;
  return f != nullptr ? f : empty_frame_info();
}

Value ActorHandle::itr_prefabs() const {
  const Entity* const e = entity();
  return e != nullptr ? e->data()->itr_prefabs() : snapshot().itr_prefabs;
}

const std::u16string& ActorHandle::bear_wpoint_attacking() const {
  static const std::u16string kEmpty;
  const Entity* const e = entity();
  if (e == nullptr) return snapshot().bear_wpoint_attacking;
  return e->bearer != nullptr ? e->bearer->frame->wpoint().attacking() : kEmpty;
}

bool ActorHandle::marks_group_attack() const {
  const Entity* const e = entity();
  if (e == nullptr) return snapshot().marks_group_attack;
  return e->marks.find(std::u16string(buff::Buff_GroupAttack::KIND)) != e->marks.end();
}

bool ActorHandle::dropping() const {
  const Entity* const e = entity();
  return e != nullptr ? e->dropping : snapshot().dropping;
}

double ActorHandle::arest() const {
  const Entity* const e = entity();
  return e != nullptr ? e->arest() : snapshot().arest;
}

bool ActorHandle::has_catcher() const {
  const Entity* const e = entity();
  return e != nullptr ? e->catcher != nullptr : snapshot().has_catcher;
}

Value ActorHandle::catcher_hurtable() const {
  const Entity* const e = entity();
  if (e == nullptr) return snapshot().catcher_hurtable;
  return e->catcher == nullptr ? Value() : Value(e->catcher->frame->cpoint().hurtable());
}

double ActorHandle::invulnerable() const {
  const Entity* const e = entity();
  return e != nullptr ? e->invulnerable() : snapshot().invulnerable;
}

Value ActorHandle::bot_ignore() const {
  const Entity* const e = entity();
  return e != nullptr ? e->bot_ignore() : snapshot().bot_ignore;
}

const std::u16string& ActorHandle::team() const {
  const Entity* const e = entity();
  return e != nullptr ? e->team() : snapshot().team;
}

const std::u16string& ActorHandle::emitter() const {
  static const std::u16string kEmpty;
  const Entity* const e = entity();
  if (e == nullptr) return snapshot().emitter;
  const std::u16string* const s = e->emitter();
  return s != nullptr ? *s : kEmpty;
}

double ActorHandle::spawn_time() const {
  const Entity* const e = entity();
  return e != nullptr ? e->spawn_time() : snapshot().spawn_time;
}

bool ActorHandle::is_bot_ctrl() const {
  const Entity* const e = entity();
  if (e == nullptr) return snapshot().is_bot_ctrl;
  return e->ctrl() != nullptr && e->ctrl()->is_bot();
}

}
}
