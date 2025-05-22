#include "depgraph.hpp"
#include "utils.hpp" // For utility functions if needed (e.g., logging, path manipulation)
#include <spdlog/spdlog.h>
#include <algorithm> // For std::reverse if needed, though current approach is fine

namespace qobs {

using namespace spdlog;

DepGraph::DepGraph(std::filesystem::path global_deps_root_path,
                   std::optional<std::string> compiler_override,
                   std::shared_ptr<Generator> generator_template)
    : m_global_deps_root_path(std::move(global_deps_root_path)),
      m_compiler_override(std::move(compiler_override)),
      m_generator_template(std::move(generator_template)) {
    debug("DepGraph initialized. Global deps root: {}", m_global_deps_root_path.string());
}

void DepGraph::add_package(const Manifest& manifest, const std::filesystem::path& src_path) {
    const std::string& package_name = manifest.package().name();
    trace("Attempting to add package: {} from path: {}", package_name, src_path.string());

    if (m_nodes.count(package_name)) {
        trace("Package {} already in graph. Skipping.", package_name);
        // Potentially check if src_path matches if multiple versions/locations were possible,
        // but for now, first one wins.
        return;
    }

    // Create and store the node. The DepGraphNode constructor sets manifest.m_package_root = src_path.
    DepGraphNode node(manifest, src_path);
    m_nodes.emplace(package_name, std::move(node)); // Use std::move if node is an rvalue
    debug("Added package {} to graph. Source: {}", package_name, src_path.string());

    // Process dependencies of the newly added package
    // Need to access the node from the map to modify its dependencies_names list
    DepGraphNode& current_node_in_map = m_nodes.at(package_name);

    for (const auto& dep_config : current_node_in_map.manifest.dependencies().m_list) {
        trace("Processing dependency '{}' for package '{}'", dep_config.name(), package_name);
        
        // Fetch dependency (this might download/clone or just return a local path)
        // The m_global_deps_root_path is where fetched archives are stored and extracted,
        // e.g., project/build/_deps/
        std::filesystem::path actual_dep_src_path = dep_config.fetch_and_get_path(m_global_deps_root_path);
        trace("Actual source path for dependency '{}': {}", dep_config.name(), actual_dep_src_path.string());

        std::filesystem::path dep_qobs_toml_path = actual_dep_src_path / "Qobs.toml";

        if (std::filesystem::exists(dep_qobs_toml_path)) {
            trace("Qobs.toml found for dependency '{}' at {}", dep_config.name(), dep_qobs_toml_path.string());
            
            // Create manifest for the dependency. Its package_root is its own src_path.
            Manifest dep_manifest(actual_dep_src_path); 
            try {
                dep_manifest.parse_file(dep_qobs_toml_path.string());
            } catch (const std::exception& e) {
                warn("Failed to parse Qobs.toml for dependency '{}' ({}): {}. Skipping as Qobs dependency.", 
                     dep_config.name(), dep_manifest.package().name(), e.what());
                continue; 
            }
            
            // The name from dep_config might be different from dep_manifest.package().name()
            // if the key in TOML is different from the package.name in the dep's Qobs.toml.
            // We should use the name from dep_manifest.package().name() as the canonical graph node key.
            const std::string& actual_dep_package_name = dep_manifest.package().name();
            if (actual_dep_package_name.empty()) {
                 warn("Dependency '{}' from {} has an empty package name in its Qobs.toml. Skipping.", 
                      dep_config.name(), actual_dep_src_path.string());
                 continue;
            }
            if (dep_config.name() != actual_dep_package_name) {
                info("Dependency alias: specified as '{}' but its Qobs.toml defines name as '{}'. Using '{}'.",
                     dep_config.name(), actual_dep_package_name, actual_dep_package_name);
            }


            // Recursively add this dependency package to the graph
            add_package(dep_manifest, actual_dep_src_path);

            // Now that the dependency is potentially in the graph, link them
            // Check if the dependency node was successfully added (it might have failed parsing or had empty name)
            if (m_nodes.count(actual_dep_package_name)) {
                 // Add actual_dep_package_name to current_node_in_map's dependencies
                current_node_in_map.dependencies_names.push_back(actual_dep_package_name);
                
                // Add current_node_in_map's name to actual_dep_package_name's dependents
                m_nodes.at(actual_dep_package_name).dependents_names.push_back(package_name);
                trace("Linked {} -> {}", package_name, actual_dep_package_name);
            } else {
                warn("Dependency package '{}' was not added to the graph. Cannot link.", actual_dep_package_name);
            }

        } else {
            trace("No Qobs.toml found for dependency '{}' at {}. Treating as non-Qobs/external.", 
                  dep_config.name(), actual_dep_src_path.string());
            // This dependency will not be a node in the graph, but its files might be used
            // via cflags/ldflags if specified directly in the current package's manifest,
            // or if the Builder class has other mechanisms for handling non-Qobs deps.
            // For DepGraph purposes, it's not a node.
        }
    }
}

std::vector<std::string> DepGraph::resolve() {
    std::vector<std::string> resolved_list;
    std::map<std::string, bool> visited;
    std::map<std::string, bool> recursion_stack;

    for (const auto& pair : m_nodes) {
        visited[pair.first] = false;
        recursion_stack[pair.first] = false;
    }

    for (const auto& pair : m_nodes) {
        if (!visited[pair.first]) {
            resolve_visit(pair.first, resolved_list, visited, recursion_stack);
        }
    }
    
    // The list is already in the correct order: dependencies come before the packages that depend on them.
    // Example: A depends on B. B is visited, B added to list. Then A is visited, A added to list. List: [B, A].
    // This means B should be built before A.
    debug("Dependency graph resolved. Build order:");
    for(size_t i = 0; i < resolved_list.size(); ++i) {
        debug("{}. {}", i + 1, resolved_list[i]);
    }
    return resolved_list;
}

void DepGraph::resolve_visit(const std::string& node_name,
                           std::vector<std::string>& resolved_list,
                           std::map<std::string, bool>& visited,
                           std::map<std::string, bool>& recursion_stack) {
    visited[node_name] = true;
    recursion_stack[node_name] = true;
    trace("Resolve visit: {}, stack depth for this path.", node_name);

    const auto& node = m_nodes.at(node_name);
    for (const std::string& dep_name : node.dependencies_names) {
        if (!m_nodes.count(dep_name)) {
            // This case should ideally not happen if add_package correctly adds all Qobs deps.
            // Could be a non-Qobs dependency that was mistakenly added to dependencies_names or a bug.
            error("Dependency '{}' for node '{}' not found in graph. This indicates an internal issue.", dep_name, node_name);
            continue;
        }
        if (!visited.at(dep_name)) {
            resolve_visit(dep_name, resolved_list, visited, recursion_stack);
        } else if (recursion_stack.at(dep_name)) {
            // Cycle detected
            std::string cycle_path = dep_name;
            std::string current = node_name;
            // Basic cycle path reconstruction (simplistic)
            // A more robust method would trace back through recursion_stack or pass path.
            // For now, just report the immediate edge causing cycle detection.
            // To make it more robust, might need to pass the current path to resolve_visit.
            // For now, this error is sufficient.
            error("Circular dependency detected: {} -> {}", node_name, dep_name);
            throw std::runtime_error(fmt::format("Circular dependency detected: {} depends on {}, which is part of the current build stack.", node_name, dep_name));
        }
    }

    recursion_stack[node_name] = false;
    resolved_list.push_back(node_name);
    trace("Resolve finished for: {}. Added to build order.", node_name);
}

} // namespace qobs
