#define _CRT_SECURE_NO_WARNINGS

#include "utils.hpp"
#include <filesystem>
#include <git2.h>
#include <iostream>
#include <subprocess.h>

using namespace spdlog;

namespace utils {

void trim_in_place(std::string& s) {
    s.erase(0, s.find_first_not_of(" \n\r\t"));
    s.erase(s.find_last_not_of(" \n\r\t") + 1);
}

void replace_in_place(std::string& s, const std::string& search,
                      const std::string& replace) {
    size_t pos = 0;
    while ((pos = s.find(search, pos)) != std::string::npos) {
        s.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

std::string replace(std::string s, const std::string& search,
                    const std::string& replace) {
    size_t pos = 0;
    while ((pos = s.find(search, pos)) != std::string::npos) {
        s.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return s;
}

bool is_directory_valid(std::string dir) {
    try {
        std::filesystem::path p(dir);
        return p.has_root_path() || p.has_relative_path();
    } catch (const std::filesystem::filesystem_error& e) {
        return false;
    }
}

std::string all_before_char(const std::string& s, char c) {
    size_t pos = s.find(c);
    if (pos == std::string::npos) {
        return s;
    }
    return s.substr(0, pos);
}

std::string toml_type_to_str(toml::node_type type) {
    std::stringstream ss;
    ss << type;
    return ss.str();
}

int popen(std::initializer_list<std::string> args) {
    // prepare argv
    std::vector<const char*> argv;
    for (auto& arg : args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);

    // spawn process
    struct subprocess_s process;
    int result = subprocess_create(argv.data(), 0, &process);
    if (result != 0)
        throw std::runtime_error(
            fmt::format("failed to create subprocess (code {})", result));

    // wait on process
    int process_return;
    result = subprocess_join(&process, &process_return);
    if (result != 0)
        throw std::runtime_error(
            fmt::format("failed to join on subprocess (code {})", result));

    return process_return;
}

// TODO: add Zig's `zig cc`
#ifdef QOBS_IS_WINDOWS
const std::vector<std::string> COMMON_C_COMPILERS = {
    "cl.exe", "clang.exe", "gcc.exe", "icx.exe", "icc.exe", "tcc.exe"};
const std::vector<std::string> COMMON_CXX_COMPILERS = {
    "cl.exe",   "clang++.exe", "g++.exe",  "clang.exe", "gcc.exe",
    "icpx.exe", "icx.exe",     "icpc.exe", "icc.exe"};
#else
const std::vector<std::string> COMMON_C_COMPILERS = {"clang", "gcc", "icx",
                                                     "icc", "tcc"};
const std::vector<std::string> COMMON_CXX_COMPILERS = {
    "clang++", "g++", "clang", "gcc", "icpx", "icx", "icpc", "icc"};
#endif

std::string find_compiler(bool need_cxx) {
    // check CC/CXX envvars
    const char* cc = std::getenv("CC");
    const char* cxx = std::getenv("CC");

    if (cc && cxx) {
        return need_cxx ? cxx : cc;
    } else if (cc) {
        return cc;
    } else if (cxx) {
        return cxx;
    } else {
        // CC/CXX envvar not set, search in PATH
        for (const auto& compiler :
             need_cxx ? COMMON_CXX_COMPILERS : COMMON_C_COMPILERS) {
            trace("trying compiler: {}", compiler);
            int result;
            try {
                result = popen({compiler, "--version"});
            } catch (const std::exception& e) {
                // this is not critical, we'll just try the next compiler
                trace("failed to spawn `{}`: {}", compiler, e.what());
                continue;
            }
            if (result == 0) {
                debug("found working compiler: {}", compiler);
                return compiler;
            } else {
                debug("compiler(?) `{}` exited with code {}", compiler, result);
            }
        }
    }
    return "";
}

template <typename... Args>
std::string ask(fmt::format_string<Args...> fmt, Args&&... args) {
    std::string answer;
    while (answer.empty()) {
        fmt::print(fmt, std::forward<Args>(args)...);
        std::getline(std::cin, answer);
    }
    return answer;
}

std::once_flag git_init_flag;
std::atomic_bool git_initialized = false;

void git_init_once() {
    std::call_once(git_init_flag, []() {
        trace("initializing libgit2");
        int result = git_libgit2_init();
        if (result < 0)
            critical("failed to initialize libgit2 (code {})", result);
        else {
            trace("libgit2 initialized ({} initialization(s))", result);
            git_initialized = true;
        }
    });
}

void maybe_shutdown_git() {
    if (git_initialized)
        git_libgit2_shutdown();
}

} // namespace utils
