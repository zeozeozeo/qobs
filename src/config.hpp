#pragma once
#include <filesystem>
#include <toml++/toml.hpp>

class Package {
public:
    Package(){};
    void parse(toml::node_view<toml::node> package);

    const std::string& name() const {
        return m_name;
    }
    const std::string& description() const {
        return m_description;
    }
    const std::vector<std::string>& authors() const {
        return m_authors;
    }
    const std::vector<std::string>& sources() const {
        return m_sources;
    }

private:
    // Package name. Field: `name`
    std::string m_name{};

    // Package description. Field: `description`
    std::string m_description{};

    // Package authors. Field: `authors`
    std::vector<std::string> m_authors{};

    std::vector<std::string> m_sources{
        std::vector<std::string>{"src/*.cpp", "src/*.cc", "src/*.c"}};
};

class Config {
public:
    Config(std::filesystem::path package_path) : m_package_path(package_path){};
    void parse_file(std::string_view config_path);
    const std::filesystem::path& package_path() const {
        return m_package_path;
    }
    const Package& package() const {
        return m_package;
    }

private:
    // Path where the config is located.
    std::filesystem::path m_package_path;

    // Parsed TOML configuration.
    toml::table m_tbl;

    // [package]
    Package m_package;
};
