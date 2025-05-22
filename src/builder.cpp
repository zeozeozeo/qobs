#include "builder.hpp"
#include "utils.hpp" // For find_compiler, etc.
#include "depgraph.hpp"
#include <glob/glob.h>
#include <spdlog/spdlog.h>
#include <fstream> // Required for std::fstream

using namespace spdlog;
// Assuming qobs namespace is used for DepGraph related items if not already global
// using namespace qobs; 

// Constructor
Builder::Builder(Manifest manifest,
                 std::filesystem::path project_build_dir,
                 std::optional<std::string> compiler_override,
                 std::shared_ptr<Generator> generator_template)
    : m_manifest(std::move(manifest)), // manifest for the root package
      m_project_build_dir(std::move(project_build_dir)),
      m_dep_graph(m_project_build_dir / "_deps", // global_deps_root_path
                  std::move(compiler_override),
                  std::move(generator_template)) {
    debug("Builder initialized for package: {}. Project build dir: {}", 
          m_manifest.package().name(), m_project_build_dir.string());
}

// Scans files for a specific package
std::vector<BuildFile> Builder::scan_files_for_package(const Manifest& pkg_manifest, const std::filesystem::path& pkg_src_path) {
    std::vector<BuildFile> files_for_pkg;
    debug("Scanning files for package {} at root {}...", pkg_manifest.package().name(), pkg_src_path.string());

    for (const auto& query : pkg_manifest.target().sources()) {
        std::filesystem::path current_scan_path = pkg_src_path / query;
        trace("Globbing query: {}", current_scan_path.string());

        std::vector<std::filesystem::path> found_files_paths;
        try {
            if (pkg_manifest.target().glob_recurse()) {
                found_files_paths = glob::rglob(current_scan_path.string());
            } else {
                found_files_paths = glob::glob(current_scan_path.string());
            }
        } catch (const std::filesystem::filesystem_error& e) {
            // Handle cases where a glob pattern might be invalid or a path doesn't exist
            // This can happen if a source path in Qobs.toml is incorrect.
            warn("Filesystem error while globbing path {}: {}. Skipping this query.", current_scan_path.string(), e.what());
            continue; 
        }


        for (const auto& p : found_files_paths) {
            trace("Found source file: {}", p.string());
            files_for_pkg.emplace_back(p); // Construct BuildFile directly
        }
    }
    debug("Queued {} file(s) for building package {}", files_for_pkg.size(), pkg_manifest.package().name());
    return files_for_pkg;
}

// Build method orchestrates using DepGraph
std::filesystem::path Builder::build(std::shared_ptr<Generator> gen_for_root_package) {
    info("Starting build process for root package: {}", m_manifest.package().name());

    // 1. Populate the dependency graph
    // The manifest for the root package (m_manifest) should have its package_root set correctly.
    // This typically happens when the Manifest object for the root project is first created (e.g. in main.cpp).
    // If m_manifest.package_root() is empty, it might default to "." which could be problematic.
    // Let's assume m_manifest.package_root() is valid.
    m_dep_graph.add_package(m_manifest, m_manifest.package_root());
    info("Dependency graph populated.");

    // 2. Resolve build order
    std::vector<std::string> build_order = m_dep_graph.resolve();
    info("Build order resolved. {} package(s) to build.", build_order.size());

    // 3. Build packages in order
    std::map<std::string, BuiltDependencyInfo> all_built_artifacts;

    for (const std::string& package_name : build_order) {
        qobs::DepGraphNode& node = m_dep_graph.get_nodes().at(package_name);
        info("Building package: {} (Type: {})", node.name, node.manifest.package().type());
        node.state = qobs::BuildState::Building;

        std::filesystem::path current_pkg_build_dir;
        if (node.name == m_manifest.package().name()) { // Root package
            current_pkg_build_dir = m_project_build_dir;
        } else { // Dependency
            current_pkg_build_dir = m_dep_graph.get_global_deps_root_path() / (node.name + "-build");
        }
        debug("Build directory for {}: {}", node.name, current_pkg_build_dir.string());
        try {
            std::filesystem::create_directories(current_pkg_build_dir);
        } catch (const std::exception& e) {
            error("Failed to create build directory {} for package {}: {}", current_pkg_build_dir.string(), node.name, e.what());
            node.state = qobs::BuildState::Failed;
            throw; // Re-throw or handle as fatal for the whole build
        }
        
        std::vector<BuildFile> current_pkg_files = scan_files_for_package(node.manifest, node.src_path);
        if (current_pkg_files.empty() && (node.manifest.package().type() == "app" || node.manifest.package().type() == "lib")) {
             warn("No source files found for package {}. It might be a header-only library or misconfigured.", node.name);
             // If it's a lib and has no sources, it might be valid (header-only).
             // If it's an app, it's an issue.
             // The generator should handle empty file list (e.g. create an empty archive for header-only lib if needed, or error for app).
        }


        std::string output_artifact_name;
        if (node.manifest.package().type() == "lib") {
            #ifdef QOBS_IS_WINDOWS
            output_artifact_name = node.name + ".lib";
            #else
            output_artifact_name = "lib" + node.name + ".a";
            #endif
        } else { // "app" or other types (if any)
            #ifdef QOBS_IS_WINDOWS
            output_artifact_name = node.name + ".exe";
            #else
            output_artifact_name = node.name;
            #endif
        }

        std::map<std::string, BuiltDependencyInfo> current_pkg_direct_deps_artifacts;
        for (const std::string& dep_node_name : node.dependencies_names) {
            if (all_built_artifacts.count(dep_node_name)) {
                current_pkg_direct_deps_artifacts[dep_node_name] = all_built_artifacts.at(dep_node_name);
            } else {
                // This should not happen if build order is correct and all deps were built
                error("INTERNAL ERROR: Dependency {} for {} was not found in all_built_artifacts.", dep_node_name, node.name);
            }
        }
        
        std::string effective_compiler = m_dep_graph.get_compiler_override().value_or(
            utils::find_compiler(node.manifest.target().m_cxx)
        );
        if (effective_compiler.empty()) {
            error("No compiler found for package {}. Searched based on CXX={}, override={}", 
                node.name, node.manifest.target().m_cxx, m_dep_graph.get_compiler_override().value_or("none"));
            node.state = qobs::BuildState::Failed;
            throw std::runtime_error("Compiler not found for " + node.name);
        }

        // TODO: Implement Generator::clone() and use it for dependencies.
        // For now, reusing gen_for_root_package for all. This is okay if NinjaGenerator is stateless for generation.
        std::shared_ptr<Generator> current_generator = gen_for_root_package;
        // if (node.name != m_manifest.package().name() && m_dep_graph.get_generator_template()) {
        //     current_generator = m_dep_graph.get_generator_template()->clone();
        // } else {
        //     current_generator = gen_for_root_package;
        // }
        // if (!current_generator) {
        //     error("No generator could be assigned for package {}", node.name);
        //     node.state = qobs::BuildState::Failed;
        //     throw std::runtime_error("Generator not available for " + node.name);
        // }


        try {
            current_generator->generate(node.manifest, current_pkg_files, output_artifact_name, 
                                        effective_compiler, current_pkg_direct_deps_artifacts, node.src_path);
            
            std::filesystem::path ninja_file_path = current_pkg_build_dir / "build.ninja";
            std::ofstream ninja_file(ninja_file_path, std::ios::out | std::ios::trunc);
            if (!ninja_file.is_open()) {
                throw std::runtime_error("Failed to open/create " + ninja_file_path.string());
            }
            ninja_file << current_generator->code();
            ninja_file.close();

            current_generator->invoke(ninja_file_path); // This might throw on failure if system() fails in invoke

            std::filesystem::path artifact_full_path = current_pkg_build_dir / output_artifact_name;
            all_built_artifacts[package_name] = {artifact_full_path, node.manifest, node.src_path};
            node.state = qobs::BuildState::Built;
            info("Successfully built package: {} -> {}", node.name, artifact_full_path.string());

        } catch (const std::exception& e) {
            error("Failed to build package {}: {}", node.name, e.what());
            node.state = qobs::BuildState::Failed;
            // Decide if we should stop the whole build or try to continue with other independent packages.
            // For now, rethrow to stop.
            throw;
        }
    }

    if (!all_built_artifacts.count(m_manifest.package().name())) {
        error("Root package {} was not found in built artifacts. Build failed.", m_manifest.package().name());
        throw std::runtime_error("Root package " + m_manifest.package().name() + " failed to build or was not processed.");
    }
    
    info("Build process completed for root package: {}", m_manifest.package().name());
    return all_built_artifacts.at(m_manifest.package().name()).artifact_path;
}
