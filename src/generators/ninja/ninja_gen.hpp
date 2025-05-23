#pragma once
#include "../generator.hpp"

class NinjaGenerator : public Generator {
public:
    NinjaGenerator(){};
    void generate(const Manifest& manifest, const std::vector<BuildFile>& files,
                  std::string_view exe_name,
                  std::string_view compiler) override;
    void invoke(std::filesystem::path path) override;
    std::string& code() override {
        return m_code;
    };

private:
    void write(std::string_view code);
    void writeln(std::string_view code = "");

    std::string m_code;
};
