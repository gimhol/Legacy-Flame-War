#include "lfw/entity/gen_field.h"

#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "lfw/base/val_expression.h"
#include "lfw/state/spawn_ice_piece.h"

namespace lfw {
namespace {

struct GenCtx {
  const FrameInfo* frame = nullptr;
  MersenneTwister* mt_ptr = nullptr;

  /** 表达式里的 `w` / `h` / `cx` / `cy`（缺失的帧字段给 NaN）。 */
  double frame_var(const std::u16string& name) const {
    if (name == u"width") return frame->width();
    if (name == u"height") return frame->height();
    if (name == u"centerx") return frame->centerx();
    if (name == u"centery") return frame->centery();
    return std::numeric_limits<double>::quiet_NaN();
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

std::optional<Value> eval_closure(GenClosure kind, const std::u16string& field,
                                  const FrameInfo& emitter_frame, MersenneTwister& mt) {
  // TS 那边闭包先 `is_object(emitter)` 判一次 —— 这里的 emitter 一定是活着的实体，
  // 帧也可能是默认构造的空帧（那时 width/height 是 NaN，与之前一致）。
  if (kind == GenClosure::BuringSmokeAction) {
    if (field != u"__gen_facing") return std::nullopt;
    const std::optional<double> got = mt.pick(std::vector<double>{-1.0, 1.0});
    return Value(got.has_value() ? *got : 0.0);
  }
  if (field == u"__gen_dvx") return state::ice_piece_dvx(mt);
  if (field == u"__gen_dvy") return state::ice_piece_dvy(mt);
  if (field == u"__gen_x") return state::ice_piece_x(emitter_frame, mt);
  if (field == u"__gen_y") return state::ice_piece_y(emitter_frame, mt);
  return std::nullopt;
}

}

void register_gen_closure(const Value& holder, GenClosure kind) {
  const std::shared_ptr<Object>* const keep = std::get_if<std::shared_ptr<Object>>(&holder);
  if (keep == nullptr || *keep == nullptr) return;
  closure_registry()[keep->get()] = ClosureSlot{*keep, kind};
}

std::optional<Value> gen_field_eval(const Value& holder, const std::u16string& kind,
                                    const FrameInfo& emitter_frame, MersenneTwister& mt) {
  GenFieldStats& stats = mutable_stats();
  ++stats.calls;
  const Object* const holder_obj = as_object(holder);
  if (holder_obj == nullptr) return std::nullopt;

  const auto& registry = closure_registry();
  const auto found = registry.find(holder_obj);
  if (found != registry.end()) {
    const std::optional<Value> got =
        eval_closure(found->second.kind, kind, emitter_frame, mt);
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
