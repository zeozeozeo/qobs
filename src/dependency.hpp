#pragma once
#include <string>
#include <toml++/toml.hpp>

enum class DependencyType {
    // supports git remotes:
    // `gh:nlohmann/json`
    // `gh:nlohmann/json#960b763`
    // `gh:nlohmann/json@3.11.3`
    // `https://github.com/nlohmann/json`
    // `https://github.com/nlohmann/json.git#960b763`
    //
    // all shortcuts: `gh:` for GitHub, `gl:` for GitLab, `bb:` for BitBucket,
    // `sr:` for sourcehut, `cb:` for Codeberg.
    git,
    // `https://example.com/my-package.zip`
    url,
    // `dep = { path = "/path/to/dep" }`
    path,
};

// Type of version string.
enum class VersionType {
    // empty version string (can be a path)
    none,
    // e.g. `960b763` or `a608bade3fc0a918a279262f2483b579ca99ca24`
    commit_hash,
    // e.g. `3.11.3` or `actually-any-tag-name`
    tag,
};

class Dependency {
public:
    Dependency(std::string name, std::string value);

    // this constructor can throw!
    Dependency(std::string name, toml::table dep,
               const std::filesystem::path& package_root);

    // throws!
    std::filesystem::path
    fetch_and_get_path(const std::filesystem::path& deps_dir);

    // `dep` in `dep = "gh:nlohmann/json"`
    inline std::string name() const {
        return m_name;
    }

    // The raw thing, as in the TOML file.
    inline std::string value() const {
        return m_value;
    }

    // Specifies the type of m_expanded, it can either be a URL or a path.
    inline DependencyType type() const {
        return m_type;
    }

    // The expanded version of the dependency, with "expanded" shortcuts,
    // without version hashes or tags.
    inline std::string expanded() const {
        return m_expanded;
    }

    // Specifies if m_version is a commit hash or version tag.
    inline VersionType version_type() const {
        return m_version_type;
    }

    // Version string or commit hash.
    inline std::string version() const {
        return m_version;
    }

private:
    // throws!
    void clone_git_repo(const std::filesystem::path& dep_path);
    void fetch_url(const std::filesystem::path& download_path);

    // `dep` in `dep = "gh:nlohmann/json"`
    std::string m_name;

    // The raw thing, as in the TOML file.
    std::string m_value;

    // Specifies the type of m_expanded, it can either be a URL or a path.
    DependencyType m_type;

    // The expanded version of the dependency, with "expanded" shortcuts,
    // without version hashes or tags.
    std::string m_expanded;

    // Specifies if m_version is a commit hash or version tag.
    VersionType m_version_type{VersionType::none};

    // Version string or commit hash.
    std::string m_version;
};
