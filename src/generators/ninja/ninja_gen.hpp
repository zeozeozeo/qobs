#pragma once
#include "../generator.hpp" // Base class
// builder.hpp is included by generator.hpp, so BuiltDependencyInfo should be known.
// No, generator.hpp includes builder.hpp, so it's fine.
#include <memory> // Required for std::unique_ptr

class NinjaGenerator : public Generator {
public:
    NinjaGenerator(){};
    // Updated signature to match Generator base class (including build_profile)
    void generate(const Manifest& manifest,
                  const std::vector<BuildFile>& files,
                  std::string_view output_name, // Was exe_name
                  std::string_view compiler,
                  const std::map<std::string, BuiltDependencyInfo>& built_dependencies,
                  const std::filesystem::path& current_package_root,
                  std::string_view build_profile) override; // Added build_profile
    void invoke(std::filesystem::path path) override;
    std::string& code() override {
        return m_code;
    };
    // Implementation of the clone method
    std::unique_ptr<Generator> clone() const override {
        return std::make_unique<NinjaGenerator>(*this);
    }

private:
    void write(std::string_view code);
    void writeln(std::string_view code = "");

    std::string m_code;
};
