#pragma once
#include "config.hpp"
#include <filesystem>

class BuildFile {
public:
    BuildFile(std::filesystem::path path) : m_path(path){};

private:
    std::filesystem::path m_path;
};

class Builder {
public:
    Builder(Config config) : m_config(config) {}
    
    void build();

private:
    void scan_files();

    Config m_config;
};
