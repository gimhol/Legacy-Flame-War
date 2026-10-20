#include "lfw/entity/gen_field.h"

#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "lfw/base/val_expression.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/state/spawn_ice_piece.h"

namespace lfw {
namespace {

struct GenCtx {
  const Value* frame = nullptr;
  MersenneTwister* mt_ptr = nullptr;

  double frame_var(const std::u16string& name) const {
    const Object* const o = as_object(*frame);
    if (o == nullptr) return std::numeric_limits<double>::quiet_NaN();
    const Value* const v = o->get(name);
    if (v == nullptr) return std::numeric_limits<double>::quiet_NaN();
    const double* const d = std::get_if<double>(v);
    return d != nullptr ? *d : std::numeric_limits<double>::quiet_NaN();
  }

  MersenneTwister& mt() const { return *mt_ptr; }
};

struct ClosureSlot {
  std::shared_ptr<Object> keep;
  GenClosure kind = GenClosure::IcePiece;
};

std::map<const Object*, ClosureSlot>& closure_registry() {
  static std::map<const Object*, ClosureSlot> kRegistry;
  return kRegistry;
}

struct CachedExpr {
  std::u16string text;
  ValExpression<GenCtx> expr;
};

std::map<std::pair<const Object*, std::u16string>, CachedExpr>& expr_cache() {
  static std::map<std::pair<const Object*, std::u16string>, CachedExpr> kCache;
  return kCache;
}

GenFieldStats& mutable_stats() {
  static GenFieldStats kStats;
  return kStats;
}

Value emitter_snapshot(const Value& emitter_frame, const Value& emitter_data) {
  auto o = std::make_shared<Object>();
  o->set(u"frame", emitter_frame);
  o->set(u"data", emitter_data);
  return Value(std::move(o));
}

std::optional<Value> eval_closure(GenClosure kind, const std::u16string& field,
                                  const Value& emitter_frame, const Value& emitter_data,
                                  MersenneTwister& mt) {
  if (kind == GenClosure::BuringSmokeAction) {
    if (field != u"__gen_facing") return std::nullopt;
    if (!entity::is_object(emitter_snapshot(emitter_frame, emitter_data))) return Value(0.0);
    const std::optional<double> got = mt.pick(std::vector<double>{-1.0, 1.0});
    return Value(got.has_value() ? *got : 0.0);
  }
  const Value emitter = emitter_snapshot(emitter_frame, emitter_data);
  if (field == u"__gen_dvx") return state::ice_piece_dvx(emitter, mt);
  if (field == u"__gen_dvy") return state::ice_piece_dvy(emitter, mt);
  if (field == u"__gen_x") return state::ice_piece_x(emitter, mt);
  if (field == u"__gen_y") return state::ice_piece_y(emitter, mt);
  return std::nullopt;
}

}

void register_gen_closure(const Value& holder, GenClosure kind) {
  const std::shared_ptr<Object>* const keep = std::get_if<std::shared_ptr<Object>>(&holder);
  if (keep == nullptr || *keep == nullptr) return;
  closure_registry()[keep->get()] = ClosureSlot{*keep, kind};
}

std::optional<Value> gen_field_eval(const Value& holder, const std::u16string& kind,
                                    const Value& emitter_frame, const Value& emitter_data,
                                    MersenneTwister& mt) {
  GenFieldStats& stats = mutable_stats();
  ++stats.calls;
  const Object* const holder_obj = as_object(holder);
  if (holder_obj == nullptr) return std::nullopt;

  const auto& registry = closure_registry();
  const auto found = registry.find(holder_obj);
  if (found != registry.end()) {
    const std::optional<Value> got =
        eval_closure(found->second.kind, kind, emitter_frame, emitter_data, mt);
    if (got.has_value()) {
      ++stats.resolved;
      ++stats.closures;
      return got;
    }
    return std::nullopt;
  }

  if (kind.size() <= 2 || kind[0] != u'_' || kind[1] != u'_') return std::nullopt;
  const std::u16string source_field = kind.substr(2);
  const Value* const source = holder_obj->get(source_field);
  if (source == nullptr) return std::nullopt;
  const std::u16string* const text = std::get_if<std::u16string>(source);
  if (text == nullptr || text->empty()) return std::nullopt;

  const std::pair<const Object*, std::u16string> key{holder_obj, kind};
  auto& cache = expr_cache();
  auto cached = cache.find(key);
  if (cached == cache.end() || cached->second.text != *text) {
    if (cache.size() >= 4096) cache.clear();
    ValExpressionOptions<GenCtx> options;
    options.tag = source_field;
    CachedExpr entry;
    entry.text = *text;
    entry.expr = ValExpression<GenCtx>(*text, options);
    cached = cache.insert_or_assign(key, std::move(entry)).first;
  }
  if (cached->second.expr.has_err) return std::nullopt;

  const GenCtx ctx{&emitter_frame, &mt};
  ++stats.resolved;
  return Value(cached->second.expr.get(ctx));
}

const GenFieldStats& gen_field_stats() { return mutable_stats(); }

}
