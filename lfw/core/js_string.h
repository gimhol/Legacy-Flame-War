#pragma once

#include <string>

namespace lfw {

double string_to_number(const std::u16string& s);
std::u16string number_to_string(double v);
std::u16string number_to_fixed_1(double v);
bool is_str_white_space(char16_t c);
std::u16string to_lower_case(const std::u16string& s);
std::u16string to_upper_case(const std::u16string& s);

}
