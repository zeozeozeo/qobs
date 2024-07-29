#include "utils.hpp"

namespace utils {

void trim_in_place(std::string& s) {
    s.erase(0, s.find_first_not_of(" \n\r\t"));
    s.erase(s.find_last_not_of(" \n\r\t") + 1);
}

} // namespace utils
