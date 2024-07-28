#include "config.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

using namespace spdlog;

bool warn_if_not_string_and_return_true(std::string_view name, size_t i,
                                        toml::node_type type) {
    if (type != toml::node_type::string) {
        std::stringstream ss;
        ss << type;
        warn("{} at index {} is of type `{}`, expected `string`", name, i,
             ss.str());
        return true;
    }
    return false;
}

void Package::parse(toml::node_view<toml::node> package) {
    if (!package["name"].is_string()) {
        throw std::runtime_error("`package.name` is required, either define it "
                                 "in Qobs.toml or re-run `qobs new`");
    }
    m_name = package["name"].as_string()->get();
    m_description = package["description"].value_or("");

    if (package["authors"].is_array()) {
        package["authors"].as_array()->for_each([this](size_t i, auto& author) {
            if (warn_if_not_string_and_return_true("author", i, author.type()))
                return;
            m_authors.push_back(author.as_string()->get());
        });
    }
}

void Target::parse(toml::node_view<toml::node> target) {
    if (target["sources"].is_array()) {
        m_sources.clear();
        target["sources"].as_array()->for_each([this](size_t i, auto& source) {
            if (warn_if_not_string_and_return_true("source", i, source.type()))
                return;
            m_sources.push_back(source.as_string()->get());
        });
    }
    m_glob_recurse = target["glob_recurse"].value_or(false);
}

void Config::parse_file(std::string_view config_path) {
    stopwatch sw;
    m_tbl = toml::parse_file(config_path);
    m_package.parse(m_tbl["package"]);
    m_target.parse(m_tbl["target"]);

    debug("config parsed in {}s. package name: `{}`, description: `{}`, "
          "authors: [{}], sources: [{}] (package path: `{}`)",
          sw, m_package.name(), m_package.description(),
          fmt::join(m_package.authors(), ", "),
          fmt::join(m_target.sources(), ", "), m_package_path.string());
}
