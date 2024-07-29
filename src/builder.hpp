#pragma once
#include "config.hpp"
#include "generators/generator.hpp"

class Builder {
public:
    Builder(Config config) : m_config(config) {}

    void build(std::shared_ptr<Generator> gen);
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
