#include "builder.hpp"
#include "utils.hpp"
#include <glob/glob.h>
#include <spdlog/spdlog.h>

using namespace spdlog;

std::filesystem::path Builder::build(std::shared_ptr<Generator> gen,
                                     std::string_view build_dir,
                                     std::optional<std::string> compiler) {
    // create build directory
    try {
        std::filesystem::create_directory(m_manifest.package_root() /
                                          build_dir);
    } catch (const std::exception& err) {
        throw std::runtime_error(
            fmt::format("couldn't create build directory: {}", err.what()));
    }

    auto build_dir_path = m_manifest.package_root() / build_dir;

    // find all package sources (this will glob `target.sources` wildcards)
    scan_files();

    // fetch & add dependencies
    handle_deps(build_dir_path);

    // generate project files
    debug("generating project files...");

    // determine exe name (FIXME: cross-compilation?)
#ifdef QOBS_IS_WINDOWS
    auto exe_name = m_manifest.package().name() + ".exe";
#else
    auto exe_name = m_manifest.package().name();
#endif

    // find cc, prefer cxx if compiling C++ package
    auto cc = compiler ? compiler.value()
                       : utils::find_compiler(m_manifest.m_target.m_cxx);
    if (cc.empty())
        throw std::runtime_error(
            "couldn't find suitable C/C++ compiler, either re-run with `-cc`, "
            "set the `CC` or `CXX` environment variable or add your compiler "
            "to PATH");

    gen->generate(m_manifest, m_files, exe_name, cc);
    trace("build.ninja:\n{}", gen->code());

    // write project files
    auto build_file_path = build_dir_path / "build.ninja";
    std::fstream file(build_file_path, std::ios::out);
    file << gen->code();
    file.close();

    // invoke generator
    gen->invoke(build_file_path);

    // return path to built file
    return build_dir_path / exe_name;
}

void Builder::scan_files() {
    debug("scanning files...");
    for (auto& query : m_manifest.target().sources()) {
        // since `qobs build` can be used with a path (e.g. `qobs build
        // package-dir`) we need to make the query relative to the path qobs is
        // being run from
        auto relative_query = m_manifest.package_root().string();
        relative_query.push_back(std::filesystem::path::preferred_separator);
        relative_query.append(query);

        // recursively glob the query
        trace("globbing relative query: {}", relative_query);
        auto files = m_manifest.target().glob_recurse()
                         ? glob::rglob(relative_query)
                         : glob::glob(relative_query);

        for (auto& p : files) {
            trace("found source file: {}", p.string());
            BuildFile file(p);
            m_files.push_back(file);
        }
    }
    debug("queued {} file(s) for building", m_files.size());
}

void Builder::handle_deps(const std::filesystem::path& build_dir_path) {
    auto deps_path = build_dir_path / "_deps";
    for (auto& dep : m_manifest.m_dependencies.m_list) {
        dep.fetch_and_get_path(deps_path);
        
    }
}
