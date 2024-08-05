#pragma once
#include "generators/generator.hpp"
#include "manifest.hpp"

class Builder {
public:
    Builder(Manifest manifest) : m_manifest(manifest) {}

    // returns path to the built executable/library
    std::filesystem::path build(std::shared_ptr<Generator> gen,
                                std::string_view build_dir,
                                std::optional<std::string> compiler);
    inline const Manifest& manifest() {
        return m_manifest;
    }
    inline const std::vector<BuildFile>& files() const {
        return m_files;
    }

private:
    void scan_files();
    void handle_deps(const std::filesystem::path& build_dir_path);

    Manifest m_manifest;
    std::vector<BuildFile> m_files;
};
