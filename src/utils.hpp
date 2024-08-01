#pragma once
#include <initializer_list>
#include <string>

#if defined(WIN32) || defined(_WIN32) ||                                       \
    defined(__WIN32) && !defined(__CYGWIN__)
#define QOBS_IS_WINDOWS
#endif

namespace utils {

void trim_in_place(std::string& s);
void replace_in_place(std::string& subject, const std::string& search,
                      const std::string& replace);
std::string replace(std::string subject, const std::string& search,
                    const std::string& replace);
int popen(std::initializer_list<std::string> args);
bool is_directory_valid(std::string dir);

} // namespace utils
