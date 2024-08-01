#pragma once
#include "../config.hpp"
#include <string>

class BuildFile {
public:
    BuildFile(std::filesystem::path path) : m_path(path){};

    const std::filesystem::path& path() const {
        return m_path;
    }

private:
    // Path to source file.
    std::filesystem::path m_path;
};

class Generator {
public:
    virtual ~Generator() = default;

    virtual void generate(const Config& config,
                          const std::vector<BuildFile>& files,
                          std::string_view exe_name) = 0;
    virtual void invoke(std::filesystem::path path){
        // nop
    };
    virtual std::string& code() = 0;
};
