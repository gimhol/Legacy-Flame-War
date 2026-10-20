#pragma once

#include "lfw/core/value.h"

namespace lfw {
namespace ui {

class UINode;
class UIComponent;

Value make_node_ref(UINode* node);
Value make_comp_ref(UIComponent* comp);
UINode* ref_to_node(const Value& v);
UIComponent* ref_to_comp(const Value& v);

}
}
