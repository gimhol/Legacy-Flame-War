#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

class World;

namespace cmds {

class CMDS;

using CmdHandler = void (*)(CMDS&);

inline bool starts_with_dash(const std::u16string& s) {
  return s.size() >= 2 && s[0] == u'-' && s[1] == u'-';
}

class CMDS {
 public:
  /** 已注册命令：名字（已小写）+ 帮助文本，按名字排序 */
  using Entry = std::pair<std::u16string, std::u16string>;

  static CmdHandler handler(const std::u16string& key);
  static void register_cmd(const std::u16string& key, const std::u16string& help, CmdHandler fn);
  static void handle(World& world, const std::vector<std::u16string>& cmds);
  static std::vector<Entry> entries();

  World& world() const { return *world_; }
  const std::u16string& cmd() const { return cmd_; }
  const std::vector<std::u16string>& words() const { return words_; }
  const std::vector<std::u16string>& positionals() const { return positionals_; }

  void set_cmd(const std::u16string& cmd);
  std::optional<std::u16string> str(size_t index) const;
  std::optional<double> num(size_t index) const;
  std::optional<std::vector<double>> nums(size_t index) const;
  std::optional<std::u16string> str_arg(const std::u16string& name) const;
  std::optional<double> num_arg(const std::u16string& name) const;
  std::optional<std::vector<double>> nums_arg(const std::u16string& name) const;

 private:
  explicit CMDS(World& world);

  const std::map<std::u16string, std::u16string>& args() const;
  std::optional<std::u16string> arg_value(const std::u16string& name) const;

  World* world_;
  std::u16string cmd_;
  std::vector<std::u16string> words_;
  std::vector<std::u16string> positionals_;
  mutable std::optional<std::map<std::u16string, std::u16string>> args_;
};

}
}
