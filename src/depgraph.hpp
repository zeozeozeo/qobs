#pragma once

#include "manifest.hpp"
#include "dependency.hpp" // For Dependency class (used by Manifest's dependencies)
#include "generators/generator.hpp" // For Generator template

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <optional>
#include <memory> // For std::shared_ptr

namespace qobs {

enum class BuildState {
    NotBuilt,
    Building,
    Built,
    Failed
};

struct DepGraphNode {
    std::string name;
    Manifest manifest;
    std::filesystem::path src_path; // Absolute path to the source code of this package
    std::vector<std::string> dependencies_names; // Names of packages it directly depends on
    std::vector<std::string> dependents_names;   // Names of packages that depend on it (inverse of dependencies_names)
    BuildState state {BuildState::NotBuilt};

    // Constructor to initialize from a manifest and its path
    DepGraphNode(Manifest m, std::filesystem::path p) 
        : manifest(std::move(m)), src_path(std::move(p)) {
        name = manifest.package().name();
        // Ensure the manifest's package_root is also set correctly if not already
        // This is crucial because Manifest::package_root() is used by Builder/Generator
        // The Manifest object itself should ideally have its package_root set upon parsing.
        // If the manifest passed here was parsed with a different root, it might need adjustment,
        // but typically manifest parsing should handle this. For a node, its src_path IS its package_root.
        // So, the manifest object stored here should reflect that.
        manifest.m_package_root = src_path; 
    }

    DepGraphNode() = default; // Needed for std::map default construction
};

class DepGraph {
public:
    DepGraph(std::filesystem::path global_deps_root_path,
             std::optional<std::string> compiler_override,
             std::shared_ptr<Generator> generator_template);

    // Adds a package (and its Qobs dependencies recursively) to the graph.
    // manifest: The manifest of the package to add.
    // src_path: The absolute path to the source code of this package.
    void add_package(const Manifest& manifest, const std::filesystem::path& src_path);

    // Resolves the build order using topological sort.
    // Detects circular dependencies.
    // Returns a vector of package names in the correct build order.
    std::vector<std::string> resolve();

    // Provides access to the nodes, e.g., for the builder to iterate after resolution.
    const std::map<std::string, DepGraphNode>& get_nodes() const { return m_nodes; }
    std::map<std::string, DepGraphNode>& get_nodes() { return m_nodes; }


    // Expose utility members if needed by the orchestrating builder
    const std::filesystem::path& get_global_deps_root_path() const { return m_global_deps_root_path; }
    const std::optional<std::string>& get_compiler_override() const { return m_compiler_override; }
    std::shared_ptr<Generator> get_generator_template() const { return m_generator_template; }


private:
    void resolve_visit(const std::string& node_name,
                       std::vector<std::string>& resolved_list,
                       std::map<std::string, bool>& visited,
                       std::map<std::string, bool>& recursion_stack);

    std::map<std::string, DepGraphNode> m_nodes;
    std::filesystem::path m_global_deps_root_path; // e.g., project_root/build/_deps
    std::optional<std::string> m_compiler_override;
    std::shared_ptr<Generator> m_generator_template; // To be used for each sub-build
};

} // namespace qobs
