#include "dependency.hpp"
#include "utils.hpp"
#include <filesystem>
#include <fmt/core.h>
#include <map>

static const std::map<std::string, std::string> SHORTCUTS{
    {"gh:", "https://github.com/"},    {"gl:", "https://gitlab.com/"},
    {"bb:", "https://bitbucket.org/"}, {"sr:", "https://sr.ht/"},
    {"cb:", "https://codeberg.org/"},
};

Dependency::Dependency(std::string name, std::string value) {
    m_name = name;
    m_value = value;

    // dep = "anything/goes" will always be a url,
    // dep = { path = "/path/to/dep" } is how you specify a path
    m_type = DependencyType::url;

    // check version, exclude it from the expanded value
    {
        auto hash_pos = value.rfind('#');
        auto tag_pos = value.rfind('@');
        if (hash_pos != std::string::npos) {
            m_version = value.substr(hash_pos + 1);
            m_expanded = value.substr(0, hash_pos);
            if (!m_version.empty())
                m_version_type = VersionType::commit_hash;
        } else if (tag_pos != std::string::npos) {
            m_version = value.substr(tag_pos + 1);
            m_expanded = value.substr(0, tag_pos);
            if (!m_version.empty())
                m_version_type = VersionType::tag;
        }
    }

    if (m_expanded.empty())
        m_expanded = value;

    // expand shortcuts
    // (e.g. `gh:nlohmann/json` -> `https://github.com/nlohmann/json`):
    for (auto&& [k, v] : SHORTCUTS) {
        if (m_expanded.starts_with(k)) {
            m_type = DependencyType::url;

            // sr.ht users start with ~, add it if not provided already
            if (k == "sr:" && !m_expanded.starts_with("sr:~"))
                m_expanded = v + '~' + m_expanded.substr(k.size());
            else
                m_expanded = v + m_expanded.substr(k.size());
            break;
        }
    }

    if (m_expanded.empty())
        m_expanded = value;
}

Dependency::Dependency(std::string name, toml::table dep,
                       const std::filesystem::path& package_root) {
    m_name = name;
    m_version_type = VersionType::none;

    for (auto&& [k, v] : dep) {
        auto key = k.str();
        if (key == "path") {
            if (!v.is_string())
                throw std::runtime_error(
                    fmt::format("`path` is of type `{}`, expected `string`",
                                utils::toml_type_to_str(v.type())));

            // expand relative paths: we don't want to read into the wrong
            // directory if `qobs build` is being run outside of the package
            // root
            std::filesystem::path p(v.as_string()->get());
            if (p.is_relative())
                p = package_root / p;

            m_value = p.string();
            m_expanded = m_value;
            m_type = DependencyType::path;
        } else {
            throw std::runtime_error(fmt::format("unrecognized key `{}`", key));
        }
    }
}
