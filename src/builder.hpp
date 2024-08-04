#pragma once
#include "config.hpp"
#include "generators/generator.hpp"

class Builder {
public:
    Builder(Config config) : m_config(config) {}

    // returns path to the built executable/library
    std::filesystem::path build(std::shared_ptr<Generator> gen,
                                std::string_view build_dir);
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
