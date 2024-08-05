#pragma once
#include "dependency.hpp"
#include <filesystem>
#include <toml++/toml.hpp>

// [package]
class Package {
public:
    Package(){};
    void parse(toml::node_view<toml::node> package);

    inline const std::string& name() const {
        return m_name;
    }
    inline const std::string& description() const {
        return m_description;
    }
    inline const std::vector<std::string>& authors() const {
        return m_authors;
    }
    void add_author(std::string author);
    std::string executable_name();

    // Package name. Field: `name`
    std::string m_name{};

    // Package description. Field: `description`
    std::string m_description{};

private:
    // Package authors. Field: `authors`
    std::vector<std::string> m_authors{};
};

// [target]
class Target {
public:
    Target(){};
    void parse(toml::node_view<toml::node> target);
    inline bool glob_recurse() const {
        return m_glob_recurse;
    }
    inline const std::vector<std::string>& sources() const {
        return m_sources;
    }
    inline const std::string& cflags() const {
        return m_cflags;
    }
    inline const std::string& ldflags() const {
        return m_ldflags;
    }

    // Prefer C++ compilers?
    bool m_cxx;

private:
    bool m_glob_recurse{true};
    std::vector<std::string> m_sources{
        std::vector<std::string>{"src/*.cpp", "src/*.cc", "src/*.c"}};

    // Compiler flags.
    std::string m_cflags;

    // Linker flags.
    std::string m_ldflags;
};

class Dependencies {
public:
    Dependencies(){};
    void parse(toml::table deps, const std::filesystem::path& package_root);

    void add(Dependency dep);
    bool has(std::string_view name, std::string_view value);

    std::vector<Dependency> m_list;
};

class Manifest {
public:
    Manifest(std::filesystem::path package_root)
        : m_package_root(package_root){};

    void parse_file(std::string_view manifest_path);
    void save_to(std::filesystem::path path);
    inline const std::filesystem::path& package_root() const {
        return m_package_root;
    }
    inline const Package& package() const {
        return m_package;
    }
    inline const Target& target() const {
        return m_target;
    }

    // [package]
    Package m_package;

    // [target]
    Target m_target;

    // [dependencies]
    Dependencies m_dependencies;

private:
    // Path where the manifest is located.
    std::filesystem::path m_package_root;

    // Parsed TOML manifest.
    toml::table m_tbl;
};
