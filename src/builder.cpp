#include "builder.hpp"
#include "utils.hpp"
#include <glob/glob.h>
#include <spdlog/spdlog.h>

using namespace spdlog;

void Builder::build(std::shared_ptr<Generator> gen) {
    // create build directory
    try {
        std::filesystem::create_directory(m_config.package_path() / "build");
    } catch (const std::exception& err) {
        throw std::runtime_error(
            fmt::format("couldn't create build directory: {}", err.what()));
    }

    // find all package sources (this will glob `target.sources` wildcards)
    scan_files();

    // generate project files
    debug("generating project files...");

    // determine exe name (FIXME: cross-compilation?)
#ifdef QOBS_IS_WINDOWS
    auto exe_name = m_config.package().name() + ".exe";
#else
    auto exe_name = m_config.package().name();
#endif

    gen->generate(m_config, m_files, exe_name);
    trace("build.ninja:\n{}", gen->code());

    // write project files
    auto build_file_path = m_config.package_path() / "build" / "build.ninja";
    std::fstream file(build_file_path, std::ios::out);
    file << gen->code();
    file.close();

    // invoke generator
    gen->invoke(build_file_path);
}

void Builder::scan_files() {
    debug("scanning files...");
    for (auto& query : m_config.target().sources()) {
        // since `qobs build` can be used with a path (e.g. `qobs build
        // package-dir`) we need to make the query relative to the path qobs is
        // being run from
        auto relative_query = m_config.package_path().string();
        relative_query.push_back(std::filesystem::path::preferred_separator);
        relative_query.append(query);

        // recursively glob the query
        trace("globbing relative query: {}", relative_query);
        auto files = m_config.target().glob_recurse()
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
