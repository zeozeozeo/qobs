#pragma once
#include "generators/generator.hpp"
#include "manifest.hpp"
#include "depgraph.hpp" // Include the new dependency graph header
#include <string>
#include <vector>
#include <filesystem>
#include <optional> // For std::optional
#include <memory>   // For std::shared_ptr

// BuiltDependencyInfo is now defined in depgraph.hpp or a shared types file.
// For this refactoring, assuming it's accessible via depgraph.hpp or globally.
// If it was previously defined here, it should be moved or #included properly.
// Based on previous steps, BuiltDependencyInfo is defined in this file,
// which is fine, but it's more logically tied to what the build process *produces*
// for each node. Let's assume it's still here for now for direct use by Builder::build.

// This struct will hold information about successfully built Qobs library dependencies.
// It's produced by the Builder (for each package) and consumed by the Generator.
// This might be better placed in a shared header or alongside DepGraphNode if it's a common output structure.
struct BuiltDependencyInfo {
    std::filesystem::path artifact_path; // Path to the compiled .a or .lib file
    Manifest manifest;                   // The manifest of the dependency (or the built package itself)
    std::filesystem::path src_path;      // The root source path of the dependency (or the built package itself)
};


class Builder {
public:
    // Constructor updated to initialize DepGraph
    Builder(Manifest manifest, 
            std::filesystem::path project_build_dir, // Used to derive global_deps_root for DepGraph
            std::optional<std::string> compiler_override,
            std::shared_ptr<Generator> generator_template);

    // Build method orchestrates using DepGraph
    // The 'build_dir' parameter for this top-level build() is where the main package itself will be built.
    // Dependencies will be built within the 'global_deps_root_path_ / <dep-name>-build /'
    std::filesystem::path build(std::shared_ptr<Generator> gen_for_root_package); // compiler_override is now a member of DepGraph

    inline const Manifest& manifest() const {
        return m_manifest; // Manifest of the root package this Builder instance was created for
    }
    // m_files is now transient per-package build, not a long-lived member of the main Builder.
    // So, files() getter might be removed or re-thought if needed.

private:
    // scan_files now takes manifest and root_path to operate on a specific package from DepGraph
    std::vector<BuildFile> scan_files_for_package(const Manifest& pkg_manifest, const std::filesystem::path& pkg_src_path);
    
    // handle_deps is removed; its logic is now within DepGraph::add_package

    Manifest m_manifest; // Manifest of the root package this Builder is primarily for
    qobs::DepGraph m_dep_graph;
    std::filesystem::path m_project_build_dir; // Build directory for the main project
                                               // (e.g. myproject/build), used to derive global_deps_root
                                               // and as the build dir for the root package itself.
    // m_files is removed as a member, will be populated locally per package.
    // m_built_dependencies is removed, will be managed as all_built_artifacts within build()
};
