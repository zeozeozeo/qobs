#pragma once
#include <string>

namespace utils {

void trim_in_place(std::string& s);
void replace_in_place(std::string& subject, const std::string& search,
                      const std::string& replace);

} // namespace utils
