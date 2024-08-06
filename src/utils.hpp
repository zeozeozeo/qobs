#pragma once
#include <initializer_list>
#include <spdlog/spdlog.h>
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

std::string toml_type_to_str(toml::node_type type);

// Throws std::runtime_error if subprocess failed to create or join.
int popen(std::initializer_list<std::string> args);

// Will return an empty string if no compiler is found.
std::string find_compiler(bool need_cxx);

template <typename... Args>
std::string ask(fmt::format_string<Args...> fmt, Args&&... args);

// libgit2 bookkeeping: initialize library once
// this will call `git_libgit2_init()` only once in the lifetime of the program,
// no matter how many times this function is called
void git_init_once();

// libgit2 bookkeeping: shutdown library if it was ever initialized
void maybe_shutdown_git();

#ifdef QOBS_IS_WINDOWS
void ensure_virtual_terminal_processing();
#endif

void init_git_repo(const std::string& path);

} // namespace utils
