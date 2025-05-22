#pragma once
#include "../manifest.hpp"
#include "../builder.hpp" // For BuiltDependencyInfo
#include <string>
#include <vector>      // Required for std::vector
#include <filesystem>  // Required for std::filesystem::path
#include <map>         // Required for std::map
#include <memory>      // Required for std::unique_ptr

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

    virtual void generate(const Manifest& manifest,
                          const std::vector<BuildFile>& files,
                          std::string_view output_name, // Was exe_name
                          std::string_view compiler,
                          const std::map<std::string, BuiltDependencyInfo>& built_dependencies,
                          const std::filesystem::path& current_package_root,
                          std::string_view build_profile) = 0; // Added build_profile
    virtual void invoke(std::filesystem::path path){
        // nop
    };
    virtual std::string& code() = 0;
    virtual std::unique_ptr<Generator> clone() const = 0; // Added clone method
};
