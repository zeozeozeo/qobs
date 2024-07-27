#include "config.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

using namespace spdlog;

void Package::parse(toml::node_view<toml::node> package) {
    m_name = package["name"].as_string()->get();
    m_description = package["description"].as_string()->value_or("");

    if (package["authors"].is_array()) {
        package["authors"].as_array()->for_each([this](auto& author) {
            m_authors.push_back(author.as_string()->get());
        });
    }

    if (package["sources"].is_array()) {
        m_sources.clear();
        package["sources"].as_array()->for_each([this](auto& source) {
            m_sources.push_back(source.as_string()->get());
        });
    }
}

void Config::parse_file(std::string_view config_path) {
    stopwatch sw;
    m_tbl = toml::parse_file(config_path);
    m_package.parse(m_tbl["package"]);

    debug("config parsed in {}s. package name: `{}`, description: `{}`, "
          "authors: [{}], sources: [{}] (package path: `{}`)",
          sw, m_package.name(), m_package.description(),
          fmt::join(m_package.authors(), ", "),
          fmt::join(m_package.sources(), ", "), m_package_path.string());
}
