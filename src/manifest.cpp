#include "manifest.hpp"
#include "utils.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <string>
#include <vector>
#include <sstream> // Required for std::stringstream

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

    // Parse package type
    if (auto type_node = package["type"]) {
        if (type_node.is_string()) {
            std::string type_val = type_node.as_string()->get();
            if (type_val == "app" || type_val == "lib") {
                m_type = type_val;
            } else {
                warn("`package.type` has invalid value `{}`, expected \"app\" or "
                     "\"lib\". Defaulting to \"app\".",
                     type_val);
                m_type = "app"; // Default
            }
        } else {
            warn("`package.type` is of type `{}`, expected `string`. Defaulting to "
                 "\"app\".",
                 utils::toml_type_to_str(type_node.type()));
            m_type = "app"; // Default
        }
    } else {
        m_type = "app"; // Default if not present
    }

    // Parse public_include_dirs
    if (auto dirs_node = package["public_include_dirs"]) {
        if (m_type == "lib") {
            if (dirs_node.is_array()) {
                dirs_node.as_array()->for_each(
                    [this](size_t i, auto& dir_node) {
                        if (warn_if_not_string_and_return_true(
                                "`package.public_include_dirs` element",
                                fmt::format("at index {}", i), dir_node.type()))
                            return;
                        m_public_include_dirs.push_back(
                            dir_node.as_string()->get());
                    });
            } else {
                warn("`package.public_include_dirs` is of type `{}`, expected "
                     "`array` of strings for library packages.",
                     utils::toml_type_to_str(dirs_node.type()));
            }
        } else { // m_type == "app"
            // Check if it's an array and not empty, or if it's not an array but simply present
            bool is_non_empty_array = dirs_node.is_array() && !dirs_node.as_array()->empty();
            bool is_present_non_array = !dirs_node.is_array(); // any other type is not expected for an app

            if (is_non_empty_array || is_present_non_array) {
                 warn("`package.public_include_dirs` is specified for an application "
                     "package (`{}`), but it's only used for library packages. This field will be ignored.", m_name);
            }
        }
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
    m_public_cflags = target["public_cflags"].value_or(""); // Parse public_cflags
    m_public_ldflags = target["public_ldflags"].value_or(""); // Parse public_ldflags
    m_cxx = target["cxx"].value_or(false);
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

void Dependencies::add(Dependency dep) {
    m_list.push_back(dep);
}

bool Dependencies::has(std::string_view name, std::string_view value) {
    for (auto& dep : m_list)
        if (dep.name() == name || dep.value() == value)
            return true;
    return false;
}

void Manifest::parse_file(std::string_view manifest_path) {
    stopwatch sw;
    m_tbl = toml::parse_file(manifest_path);
    m_package.parse(m_tbl["package"]);
    m_target.parse(m_tbl["target"]);

    auto deps = m_tbl["dependencies"];
    if (deps.is_table())
        m_dependencies.parse(*deps.as_table(), m_package_root);
    else
        warn("`dependencies` is of type `{}`, expected `table`",
             utils::toml_type_to_str(deps.type()));

    debug("manifest parsed in {}s. package name: `{}`, description: `{}`, "
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

void Manifest::save_to(std::filesystem::path path) {
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
    if (m_package.type() == "lib") { // Only write type if it's "lib"
        file << fmt_field("type", m_package.type()) << "\n";
    }
    if (!m_package.authors().empty()) {
        file << "authors = " << fmt_vector(m_package.authors()) << "\n";
    }
    if (m_package.type() == "lib" && !m_package.public_include_dirs().empty()) {
        file << "public_include_dirs = " << fmt_vector(m_package.public_include_dirs()) << "\n";
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
    if (!m_target.public_cflags().empty()) { // Serialize public_cflags
        file << fmt_field("public_cflags", m_target.public_cflags()) << "\n";
    }
    if (!m_target.public_ldflags().empty()) { // Serialize public_ldflags
        file << fmt_field("public_ldflags", m_target.public_ldflags()) << "\n";
    }
    file << fmt_field("cxx", m_target.m_cxx) << "\n";

    // [dependencies]
    file << "\n[dependencies]\n";
    for (auto& dep : m_dependencies.m_list) {
        switch (dep.type()) {
        case DependencyType::git:
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
