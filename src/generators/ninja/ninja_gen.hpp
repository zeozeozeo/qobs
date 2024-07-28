#pragma once
#include "../../builder.hpp"
#include "../generator.hpp"

class NinjaGenerator : public Generator {
public:
    NinjaGenerator(Builder builder) : m_builder(builder){};
    void generate() override;
    std::string& code() override {
        return m_code;
    };

private:
    void write(std::string code);
    void writeln(std::string code);

    Builder m_builder;
    std::string m_code;
};
