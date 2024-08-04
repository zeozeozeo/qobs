#pragma once
#include <optional>
#include <string>
#include <toml++/toml.hpp>

#if defined(WIN32) || defined(_WIN32) ||                                       \
    defined(__WIN32) && !defined(__CYGWIN__)
#define QOBS_IS_WINDOWS
#endif

namespace utils {

void trim_in_place(std::string& s);

void replace_in_place(std::string& s, const std::string& search,
                      const std::string& replace);

std::string replace(std::string s, const std::string& search,
                    const std::string& replace);

// int popen(std::initializer_list<std::string> args);

bool is_directory_valid(std::string dir);

std::pair<std::string, std::optional<std::string>>
rsplit_once(const std::string& s, char delimiter);

std::string toml_type_to_str(toml::node_type type);

} // namespace utils
