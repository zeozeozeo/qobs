#include "builder.hpp"
#include <glob/glob.h>
#include <spdlog/spdlog.h>

using namespace spdlog;

void Builder::build() {
    scan_files();
}

void Builder::scan_files() {
    debug("scanning files...");
    for (auto& query : m_config.package().sources()) {
        // since `qobs build` can be used with a path (e.g. `qobs build
        // package-dir`) we need to make the query relative to the path qobs is
        // being run from
        auto relative_query = m_config.package_path().string();
        relative_query.push_back(std::filesystem::path::preferred_separator);
        relative_query.append(query);
        trace("relative query: {}", relative_query);

        for (auto& p : glob::rglob(relative_query)) {
            trace("found source file: {}", p.string());
        }
    }
}
