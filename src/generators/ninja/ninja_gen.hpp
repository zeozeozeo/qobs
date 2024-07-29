#pragma once
#include "../generator.hpp"

class NinjaGenerator : public Generator {
public:
    NinjaGenerator(){};
    void generate(const Config& config,
                  const std::vector<BuildFile>& files) override;
    std::string& code() override {
        return m_code;
    };

private:
    void write(std::string code);
    void writeln(std::string code);

    std::string m_code;
};
