#pragma once
#include <string>

class Generator {
public:
    virtual ~Generator() = default;

    virtual void generate() = 0;
    virtual std::string& code() = 0;
};
