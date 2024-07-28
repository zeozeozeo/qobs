#pragma once
#include "../../builder.hpp"
#include "../generator.hpp"

const std::string DEFAULT_CODE = "";

class NinjaGenerator : public Generator {
public:
    NinjaGenerator(Builder builder) : m_builder(builder){};
    void generate() override;
    std::string& code() override {
        return m_code;
    };

private:
    Builder m_builder;
    std::string m_code{DEFAULT_CODE};
};
