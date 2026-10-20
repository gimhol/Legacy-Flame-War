#pragma once

#include <functional>
#include <string>
#include <utility>

#include "lfw/core/value.h"
#include "lfw/utils/times.h"

namespace lfw {

class LFW;

namespace ui {

class UINode;

class IUIImgLoaderNode {
 public:
  virtual ~IUIImgLoaderNode() = default;
  virtual LFW& lfw() = 0;
  virtual void set_image(const Value& image) = 0;
  virtual void resize(double w, double h) = 0;
};

struct UIImgLoadResult {
  bool ok = false;
  bool out_of_date = false;
  Value texture;
  Value image;
  std::u16string error;
  bool error_is_error = true;
};

class UIImgLoader {
 public:
  using NodeGetter = std::function<IUIImgLoaderNode*()>;

  explicit UIImgLoader(NodeGetter node) : _node(std::move(node)) {}

  UIImgLoader& ignore_out_of_date();
  UIImgLoadResult load(const Value& uiimg);
  UIImgLoadResult set_img(const std::u16string& path);

  const Times& jid() const { return _jid; }

 private:
  NodeGetter _node;
  Times _jid;
};

class UINodeImgAdapter : public IUIImgLoaderNode {
 public:
  explicit UINodeImgAdapter(std::function<UINode*()> get) : _get(std::move(get)) {}

  LFW& lfw() override;
  void set_image(const Value& image) override;
  void resize(double w, double h) override;

 private:
  std::function<UINode*()> _get;
};

}
}
