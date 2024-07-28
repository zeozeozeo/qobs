#pragma once
#include "config.hpp"

class BuildFile {
public:
    BuildFile(std::filesystem::path path) : m_path(path){};

    const std::filesystem::path& path() const {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

class Builder {
public:
    Builder(Config config) : m_config(config) {}

    void build();
    Config& config() {
        return m_config;
    }
    const std::vector<BuildFile>& files() const {
        return m_files;
    }

private:
    void scan_files();
    Config m_config;
    std::vector<BuildFile> m_files;
};
