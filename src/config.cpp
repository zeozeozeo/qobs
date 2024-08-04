#include "config.hpp"
#include "utils.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

using namespace spdlog;

bool warn_if_not_string_and_return_true(std::string_view name,
                                        std::string_view where,
                                        toml::node_type type) {
    if (type != toml::node_type::string) {
        warn("{} {} is of type `{}`, expected `string`", name, where,
             utils::toml_type_to_str(type));
        return true;
    }
    return false;
}

void Package::parse(toml::node_view<toml::node> package) {
    if (!package["name"].is_string()) {
        throw std::runtime_error(
            "`package.name` is required, either define it "
            "in Qobs.toml or re-run `qobs new`:\n[package]\nname = "
            "\"my-package-name\" # this is required");
    }
    m_name = package["name"].as_string()->get();
    m_description = package["description"].value_or("");

    if (package["authors"].is_array()) {
        package["authors"].as_array()->for_each([this](size_t i, auto& author) {
            if (warn_if_not_string_and_return_true(
                    "author", fmt::format("at index {}", i), author.type()))
                return;
            m_authors.push_back(author.as_string()->get());
        });
    }
}

void Package::add_author(std::string author) {
    m_authors.push_back(author);
}

void Target::parse(toml::node_view<toml::node> target) {
    if (target["sources"].is_array()) {
        m_sources.clear();
        target["sources"].as_array()->for_each([this](size_t i, auto& source) {
            if (warn_if_not_string_and_return_true(
                    "source", fmt::format("at index {}", i), source.type()))
                return;
            m_sources.push_back(source.as_string()->get());
        });
    }
    m_glob_recurse = target["glob_recurse"].value_or(false);
    m_cflags = target["cflags"].value_or("");
    m_ldflags = target["ldflags"].value_or("");
}

void Dependencies::parse(toml::table deps,
                         const std::filesystem::path& package_root) {
    size_t i = 0;
    for (auto&& [k, v] : deps) {
        // since `k.str()` is a string_view we need to copy it into a string
        std::string key_str = {k.str().begin(), k.str().end()};

        // dep = { path = "/path/to/dep/" }
        if (v.is_table()) {
            try {
                m_list.push_back(
                    Dependency(key_str, *v.as_table(), package_root));
            } catch (const std::exception& e) {
                warn("couldn't parse dependency `{}` at index {}: {}", k.str(),
                     i, e.what());
            }
            ++i;
            continue;
        }

        // dep = "gh:fmtlib/fmt"
        if (warn_if_not_string_and_return_true(
                "dependency", fmt::format("`{}` at index {}", k.str(), i),
                v.type()))
            continue;

        auto dep = Dependency(key_str, v.as_string()->get());
        trace("dependency: value = `{}`, expanded = `{}`, version = `{}`",
              dep.value(), dep.expanded(), dep.version());

        m_list.push_back(dep);
        ++i;
    }
}

void Config::parse_file(std::string_view config_path) {
    stopwatch sw;
    m_tbl = toml::parse_file(config_path);
    m_package.parse(m_tbl["package"]);
    m_target.parse(m_tbl["target"]);

    auto deps = m_tbl["dependencies"];
    if (deps.is_table())
        m_dependencies.parse(*deps.as_table(), m_package_root);
    else
        warn("`dependencies` is of type `{}`, expected `table`",
             utils::toml_type_to_str(deps.type()));

    debug("config parsed in {}s. package name: `{}`, description: `{}`, "
          "authors: [{}], sources: [{}] (package path: `{}`)",
          sw, m_package.name(), m_package.description(),
          fmt::join(m_package.authors(), ", "),
          fmt::join(m_target.sources(), ", "), m_package_root.string());
}

template <typename T> std::string fmt_vector(const std::vector<T>& vec) {
    toml::array arr;
    for (auto& v : vec) {
        arr.push_back(v);
    }

    std::stringstream ss;
    ss << arr;
    auto str = ss.str();

    // reformat ugly arrays:
    // `[ 1, 2 ]` => `[1, 2]`
    utils::replace_in_place(str, "[ ", "[");
    utils::replace_in_place(str, " ]", "]");
    return str;
}

template <typename T> std::string fmt_field(std::string_view name, T&& value) {
    std::stringstream ss;
    ss << toml::table{{name, std::forward<T>(value)}};
    auto str = ss.str();

    // reformat ugly arrays:
    // `value = [ 1, 2 ]` => `value = [1, 2]`
    utils::replace_in_place(str, "[ ", "[");
    utils::replace_in_place(str, " ]", "]");
    return str;
}

void Config::save_to(std::filesystem::path path) {
    // obviously we could use the toml++ serializer all of this, but
    // it uses std::map under the hood, which stores keys lexicographically,
    // e.g. package.author comes before package.name in the saved file, which
    // doesn't look that good, so we only use it for serializing invididual keys

    std::ofstream file(path, std::ios::out | std::ios::trunc);

    // [package]
    file << "[package]\n";
    file << fmt_field("name", m_package.name()) << "\n";
    if (!m_package.description().empty()) {
        file << fmt_field("description", m_package.description()) << "\n";
    }
    if (!m_package.authors().empty()) {
        file << "authors = " << fmt_vector(m_package.authors()) << "\n";
    }

    // [target]
    file << "\n[target]\n";
    if (!m_target.glob_recurse()) {
        file << fmt_field("glob_recurse", m_target.glob_recurse()) << "\n";
    }
    file << "sources = " << fmt_vector(m_target.sources()) << "\n";
    if (!m_target.cflags().empty()) {
        file << fmt_field("cflags", m_target.cflags()) << "\n";
    }
    if (!m_target.ldflags().empty()) {
        file << fmt_field("ldflags", m_target.ldflags()) << "\n";
    }

    // [dependencies]
    file << "\n[dependencies]\n";
    for (auto& dep : m_dependencies.list()) {
        switch (dep.type()) {
        case DependencyType::url:
            file << fmt_field(dep.name(), dep.value()) << "\n";
            break;
        case DependencyType::path:
            toml::table tbl{{"path", dep.value()}};
            file << fmt_field(dep.name(), tbl) << "\n";
            break;
        }
    }

    file.close();
}
