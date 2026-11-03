#pragma once

#include <string>

namespace lfw::host::net {

/** 联机地基自检：同步策略、协议编解码、两种驱动在回环网络下的行为 */
struct SelftestReport {
  bool ok = false;
  std::string text;
};

SelftestReport run_selftest();

}
