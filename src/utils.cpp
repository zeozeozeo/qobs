#include "utils.hpp"
#include <filesystem>

namespace utils {

void trim_in_place(std::string& s) {
    s.erase(0, s.find_first_not_of(" \n\r\t"));
    s.erase(s.find_last_not_of(" \n\r\t") + 1);
}

void replace_in_place(std::string& s, const std::string& search,
                      const std::string& replace) {
    size_t pos = 0;
    while ((pos = s.find(search, pos)) != std::string::npos) {
        s.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}
std::string replace(std::string s, const std::string& search,
                    const std::string& replace) {
    size_t pos = 0;
    while ((pos = s.find(search, pos)) != std::string::npos) {
        s.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return s;
}

// int popen(std::initializer_list<std::string> args) {
//     std::vector<char*> vc;
//     std::transform(args.begin(), args.end(), std::back_inserter(vc),
//                    [](const std::string& s) {
//                        char* pc = new char[s.size() + 1];
//                        std::strcpy(pc, s.c_str());
//                        return pc;
//                    });
//
//     struct subprocess_s subprocess;
//     int result = subprocess_create(&vc[0], 0, &subprocess);
//
//     for (size_t i = 0; i < vc.size(); i++)
//         delete[] vc[i];
//
//     return result;
// }

bool is_directory_valid(std::string dir) {
    try {
        std::filesystem::path p(dir);
        return p.has_root_path() || p.has_relative_path();
    } catch (const std::filesystem::filesystem_error& e) {
        return false;
    }
}

std::string all_before_char(const std::string& s, char c) {
    size_t pos = s.find(c);
    if (pos == std::string::npos) {
        return s;
    }
    return s.substr(0, pos);
}

std::pair<std::string, std::optional<std::string>>
rsplit_once(const std::string& s, char delimiter) {
    size_t pos = s.rfind(delimiter);
    if (pos == std::string::npos) {
        return {s, std::nullopt};
    } else {
        return {s.substr(0, pos), s.substr(pos + 1)};
    }
}

std::string toml_type_to_str(toml::node_type type) {
    std::stringstream ss;
    ss << type;
    return ss.str();
}

} // namespace utils
