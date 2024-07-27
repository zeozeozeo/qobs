#pragma once
#include <filesystem>

class Generator {
public:
    virtual void add_file(std::filesystem::path path) = 0;
};
