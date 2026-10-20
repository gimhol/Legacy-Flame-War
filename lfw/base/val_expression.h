#pragma once

#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lfw/core/js_num.h"
#include "lfw/core/js_string.h"
#include "lfw/utils/math/base.h"
#include "lfw/utils/math/mersenne_twister.h"

namespace lfw {

template <typename Ctx>
using ValGetterFn = std::function<double(const Ctx&)>;

template <typename Ctx>
struct ValExpressionOptions {
  std::u16string tag;
  std::map<std::u16string, ValGetterFn<Ctx>> vars;
};

namespace val_expr_detail {

inline bool is_digit(char16_t c) { return c >= u'0' && c <= u'9'; }

inline bool is_ident_char(char16_t c) {
  return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || (c >= u'0' && c <= u'9') ||
         c == u'_';
}

inline const std::vector<double>& flip_values() {
  static const std::vector<double> kValues{-1.0, 1.0};
  return kValues;
}

inline double undefined_number() { return std::numeric_limits<double>::quiet_NaN(); }

inline std::u16string dec(std::size_t v) {
  if (v == 0) return u"0";
  std::u16string out;
  while (v > 0) {
    out.insert(out.begin(), static_cast<char16_t>(u'0' + (v % 10)));
    v /= 10;
  }
  return out;
}

}

template <typename Ctx>
class ValExpression {
 public:
  ValExpression() = default;

  explicit ValExpression(const std::u16string& source,
                         const ValExpressionOptions<Ctx>& options = {}) {
    tag = options.tag.empty() ? std::u16string(u"val_expr") : options.tag;
    for (char16_t c : source) {
      if (!is_str_white_space(c)) text.push_back(c);
    }
    Parse(options.vars);
  }

  double get(const Ctx& e) const { return _get(e); }

  std::u16string text;
  std::u16string tag;
  std::u16string err;
  bool has_err = false;

 private:
  using Get = ValGetterFn<Ctx>;

  static const std::map<std::u16string, Get>& default_vars() {
    static const std::map<std::u16string, Get> kVars{
        {u"w", [](const Ctx& e) { return e.frame_var(u"width"); }},
        {u"h", [](const Ctx& e) { return e.frame_var(u"height"); }},
        {u"cx", [](const Ctx& e) { return e.frame_var(u"centerx"); }},
        {u"cy", [](const Ctx& e) { return e.frame_var(u"centery"); }},
    };
    return kVars;
  }

  std::optional<char16_t> at(std::size_t i) const {
    if (i >= text.size()) return std::nullopt;
    return text[i];
  }

  void fail(const std::u16string& message) {
    has_err = true;
    err = u"[ValExpression] " + tag + u": " + message + u" @" + val_expr_detail::dec(_index) +
          u" in \"" + text + u"\"";
  }

  void Parse(const std::map<std::u16string, Get>& custom_vars) {
    _vars = default_vars();
    for (const auto& kv : custom_vars) _vars[kv.first] = kv.second;

    if (text.empty()) {
      fail(u"empty expression");
      return;
    }
    const Get ret = ParseExpr();
    if (has_err) return;
    if (_index < text.size()) {
      fail(u"unexpected '" + text.substr(_index) + u"'");
      return;
    }
    if (ret != nullptr) _get = ret;
  }

  Get ParseExpr() {
    Get ret = ParseTerm();
    if (has_err) return nullptr;
    for (;;) {
      const std::optional<char16_t> op = at(_index);
      if (op != u'+' && op != u'-') return ret;
      ++_index;
      const Get right = ParseTerm();
      if (has_err) return nullptr;
      const Get l = ret;
      const Get r = right;
      ret = *op == u'+' ? Get([l, r](const Ctx& e) {
        const double a = l(e);
        const double b = r(e);
        return a + b;
      })
                        : Get([l, r](const Ctx& e) {
                            const double a = l(e);
                            const double b = r(e);
                            return a - b;
                          });
    }
  }

  Get ParseTerm() {
    Get ret = ParseUnary();
    if (has_err) return nullptr;
    for (;;) {
      const std::optional<char16_t> op = at(_index);
      if (op != u'*' && op != u'/') return ret;
      ++_index;
      const Get right = ParseUnary();
      if (has_err) return nullptr;
      const Get l = ret;
      const Get r = right;
      ret = *op == u'*' ? Get([l, r](const Ctx& e) {
        const double a = l(e);
        const double b = r(e);
        return a * b;
      })
                        : Get([l, r](const Ctx& e) {
                            const double a = l(e);
                            const double b = r(e);
                            return a / b;
                          });
    }
  }

  Get ParseUnary() {
    const std::optional<char16_t> c = at(_index);
    if (c != u'-') return ParsePrimary();
    ++_index;
    const Get v = ParseUnary();
    if (has_err) return nullptr;
    return Get([v](const Ctx& e) { return -v(e); });
  }

  Get ParsePrimary() {
    const std::optional<char16_t> c = at(_index);
    if (!c.has_value()) {
      fail(u"unexpected end of expression");
      return nullptr;
    }
    if (*c == u'(') {
      ++_index;
      const Get v = ParseExpr();
      if (has_err) return nullptr;
      if (at(_index) != u')') {
        fail(u"missing ')'");
        return nullptr;
      }
      ++_index;
      return v;
    }
    if (val_expr_detail::is_digit(*c) || *c == u'.') return ParseNumber();
    if (val_expr_detail::is_ident_char(*c)) return ParseIdent();
    fail(u"unexpected character '" + std::u16string(1, *c) + u"'");
    return nullptr;
  }

  Get ParseNumber() {
    const std::size_t start = _index;
    for (;;) {
      const std::optional<char16_t> c = at(_index);
      if (!c.has_value() || !val_expr_detail::is_digit(*c)) break;
      ++_index;
    }
    if (at(_index) == u'.') {
      ++_index;
      for (;;) {
        const std::optional<char16_t> c = at(_index);
        if (!c.has_value() || !val_expr_detail::is_digit(*c)) break;
        ++_index;
      }
    }
    const std::u16string raw = text.substr(start, _index - start);
    const double value = string_to_number(raw);
    if (!std::isfinite(value)) {
      fail(u"bad number '" + raw + u"'");
      return nullptr;
    }
    return Get([value](const Ctx&) { return value; });
  }

  Get ParseIdent() {
    const std::size_t start = _index;
    for (;;) {
      const std::optional<char16_t> c = at(_index);
      if (!c.has_value() || !val_expr_detail::is_ident_char(*c)) break;
      ++_index;
    }
    const std::u16string name = text.substr(start, _index - start);
    if (at(_index) != u'(') {
      const auto it = _vars.find(name);
      if (it == _vars.end()) {
        fail(u"unknown identifier '" + name + u"'");
        return nullptr;
      }
      return it->second;
    }
    ++_index;
    std::vector<Get> args;
    if (at(_index) != u')') {
      args.push_back(ParseExpr());
      if (has_err) return nullptr;
      while (at(_index) == u',') {
        ++_index;
        args.push_back(ParseExpr());
        if (has_err) return nullptr;
      }
    }
    if (at(_index) != u')') {
      fail(u"missing ')' for '" + name + u"(...'");
      return nullptr;
    }
    ++_index;
    return ParseCall(name, args);
  }

  Get ParseCall(const std::u16string& name, const std::vector<Get>& args) {
    const std::u16string tag_snapshot = tag;
    if (name == u"rand") {
      if (args.size() != 2) {
        fail(u"'rand' expects 2 arguments, got " + val_expr_detail::dec(args.size()));
        return nullptr;
      }
      const Get a = args[0];
      const Get b = args[1];
      return Get([tag_snapshot, a, b](const Ctx& e) -> double {
        MersenneTwister& mt = e.mt();
        mt.mark = tag_snapshot;
        const double lo = a(e);
        const double hi = b(e);
        return mt.range(lo, hi);
      });
    }
    if (name == u"pick") {
      if (args.empty()) {
        fail(u"'pick' expects at least 1 argument");
        return nullptr;
      }
      const std::shared_ptr<std::vector<double>> scratch =
          std::make_shared<std::vector<double>>(args.size());
      return Get([tag_snapshot, args, scratch](const Ctx& e) -> double {
        for (std::size_t i = 0; i < args.size(); ++i) (*scratch)[i] = args[i](e);
        MersenneTwister& mt = e.mt();
        mt.mark = tag_snapshot;
        const std::optional<double> v = mt.pick(*scratch);
        return v.has_value() ? *v : val_expr_detail::undefined_number();
      });
    }
    if (name == u"bag") {
      if (args.empty()) {
        fail(u"'bag' expects at least 1 argument");
        return nullptr;
      }
      const std::shared_ptr<std::vector<double>> scratch =
          std::make_shared<std::vector<double>>(args.size());
      const std::shared_ptr<std::vector<double>> cur = std::make_shared<std::vector<double>>();
      const std::shared_ptr<std::optional<double>> taken = std::make_shared<std::optional<double>>();
      return Get([tag_snapshot, args, scratch, cur, taken](const Ctx& e) -> double {
        for (std::size_t i = 0; i < args.size(); ++i) (*scratch)[i] = args[i](e);
        if (cur->empty()) {
          cur->clear();
          if (args.size() > 1) {
            for (double v : *scratch) {
              if (!taken->has_value() || v != **taken) cur->push_back(v);
            }
          } else {
            *cur = *scratch;
          }
          if (cur->empty()) *cur = *scratch;
        }
        MersenneTwister& mt = e.mt();
        mt.mark = tag_snapshot;
        const double idx = mt.range(0.0, static_cast<double>(cur->size()));
        const std::size_t i = static_cast<std::size_t>(js_to_uint32(idx));
        const double got = (*cur)[i];
        cur->erase(cur->begin() + static_cast<std::ptrdiff_t>(i));
        *taken = got;
        return got;
      });
    }
    if (name == u"flip") {
      if (!args.empty()) {
        fail(u"'flip' expects no arguments");
        return nullptr;
      }
      return Get([tag_snapshot](const Ctx& e) -> double {
        MersenneTwister& mt = e.mt();
        mt.mark = tag_snapshot;
        const std::optional<double> v = mt.pick(val_expr_detail::flip_values());
        return v.has_value() ? *v : val_expr_detail::undefined_number();
      });
    }
    if (name == u"round") {
      if (args.size() != 1) {
        fail(u"'round' expects 1 argument, got " + val_expr_detail::dec(args.size()));
        return nullptr;
      }
      const Get a = args[0];
      return Get([a](const Ctx& e) { return round(a(e)); });
    }
    fail(u"unknown function '" + name + u"'");
    return nullptr;
  }

  std::map<std::u16string, Get> _vars;
  std::size_t _index = 0;
  Get _get = [](const Ctx&) { return 0.0; };
};

}
