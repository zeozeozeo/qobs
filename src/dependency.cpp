#include "dependency.hpp"
#include "utils.hpp"
#include <deque>
#include <filesystem>
#include <fmt/core.h>
#include <git2.h>
#include <indicators/dynamic_progress.hpp>
#include <indicators/progress_bar.hpp>
#include <map>

using namespace spdlog;
using namespace indicators;

static const std::map<std::string, std::string> SHORTCUTS{
    {"gh:", "https://github.com/"},    {"gl:", "https://gitlab.com/"},
    {"bb:", "https://bitbucket.org/"}, {"sr:", "https://sr.ht/"},
    {"cb:", "https://codeberg.org/"},
};

Dependency::Dependency(std::string name, std::string value) {
    m_name = name;
    m_value = value;

    // dep = "anything/goes" will always be a url or git remote,
    // dep = { path = "/path/to/dep" } is how you specify a path
    m_type = DependencyType::url;

    // check version, exclude it from the expanded value
    {
        auto hash_pos = value.rfind('#');
        auto tag_pos = value.rfind('@');

        if (hash_pos != std::string::npos) {
            m_type = DependencyType::git;
            m_version = value.substr(hash_pos + 1);
            m_expanded = value.substr(0, hash_pos);
            if (!m_version.empty())
                m_version_type = VersionType::commit_hash;
        } else if (tag_pos != std::string::npos) {
            m_type = DependencyType::git;
            m_version = value.substr(tag_pos + 1);
            m_expanded = value.substr(0, tag_pos);
            if (!m_version.empty())
                m_version_type = VersionType::tag;
        }
    }

    if (m_expanded.empty())
        m_expanded = value;

    // expand shortcuts
    // (e.g. `gh:nlohmann/json` -> `https://github.com/nlohmann/json`):
    for (auto&& [k, v] : SHORTCUTS) {
        if (m_expanded.starts_with(k)) {
            m_type = DependencyType::git; // shortcut to git remote

            // sr.ht users start with ~, add it if not provided already
            if (k == "sr:" && !m_expanded.starts_with("sr:~"))
                m_expanded = v + '~' + m_expanded.substr(k.size());
            else
                m_expanded = v + m_expanded.substr(k.size());
            break;
        }
    }

    // m_type could be DependencyType::git OR DependencyType::url by now

    if (m_expanded.empty())
        m_expanded = value;
}

Dependency::Dependency(std::string name, toml::table dep,
                       const std::filesystem::path& package_root) {
    m_name = name;
    m_version_type = VersionType::none;

    for (auto&& [k, v] : dep) {
        auto key = k.str();
        if (key == "path") {
            if (!v.is_string())
                throw std::runtime_error(
                    fmt::format("`path` is of type `{}`, expected `string`",
                                utils::toml_type_to_str(v.type())));

            // expand relative paths: we don't want to read into the wrong
            // directory if `qobs build` is being run outside of the package
            // root
            std::filesystem::path p(v.as_string()->get());
            if (p.is_relative())
                p = package_root / p;

            m_value = p.string();
            m_expanded = m_value;
            m_type = DependencyType::path;
        } else {
            throw std::runtime_error(fmt::format("unrecognized key `{}`", key));
        }
    }
}

// Kinda like DynamicProgress from the `indicators` library, but doesn't hog
// performance when running in Windows cmd. Handles sequencing of multiple
// progressbars, update granularity and forces Windows to process ANSI escape
// codes.
class SequentialProgress {
public:
    SequentialProgress() {}

    // Sequence a progressbar. It will only start once all progressbars before
    // it have completed, and after that `update_progress` will update this
    // sequenced progressbar.
    void add_bar(std::unique_ptr<ProgressBar> bar) {
#ifdef QOBS_IS_WINDOWS
        // see https://github.com/p-ranav/indicators/issues/131
        utils::ensure_virtual_terminal_processing();
#endif
        m_bars.push_back(std::move(bar));
    }

    // Update progress of the current progressbar in the sequence. Does nothing
    // if the sequence is empty/becomes empty.
    void update_progress(double progress) {
        if (m_bars.empty())
            return;
        auto bar = m_bars[0].get();
        if (bar->is_completed() && progress != 1.L) {
            m_bars.pop_front();
            if (m_bars.empty())
                return;
            bar = m_bars[0].get();
        }

        auto progress_i = static_cast<size_t>(progress * 100.L);
        if (bar->current() != progress_i)
            bar->set_progress(progress_i);
    }

private:
    std::deque<std::unique_ptr<ProgressBar>> m_bars{};
};

static int sideband_progress(const char* str, int len, void* payload) {
    (void)payload; // unused
    printf("  remote: %.*s", len, str);
    std::flush(std::cout);
    return 0;
}

static int fetch_progress(const git_indexer_progress* stats, void* payload) {
    auto sp = static_cast<SequentialProgress*>(payload);
    auto progress = static_cast<double>(stats->received_objects) /
                    static_cast<double>(stats->total_objects);
    sp->update_progress(progress);
    return 0;
}

void checkout_progress(const char* path, size_t cur, size_t tot,
                       void* payload) {
    auto sp = static_cast<SequentialProgress*>(payload);
    auto progress = static_cast<double>(cur) / static_cast<double>(tot);

    // technically this should only run after `fetch_progress` is done, so this
    // will update the second progressbar
    sp->update_progress(progress);
}

void Dependency::clone_git_repo(const std::filesystem::path& dep_path) {
    // setup progressbars
    auto fetch_bar = std::make_unique<ProgressBar>(
        option::BarWidth{50}, option::PrefixText{"  fetching "},
        option::ForegroundColor{Color::green}, option::ShowElapsedTime{true},
        option::ShowRemainingTime{true},
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}});
    auto checkout_bar = std::make_unique<ProgressBar>(
        option::BarWidth{50}, option::PrefixText{"  checkout "},
        option::ForegroundColor{Color::yellow}, option::ShowElapsedTime{true},
        option::ShowRemainingTime{true},
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}});

    // sequence 2 progressbars:
    SequentialProgress sp;
    sp.add_bar(std::move(fetch_bar));
    sp.add_bar(std::move(checkout_bar));

    git_repository* cloned_repo = nullptr;
    git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;

    auto url = m_expanded.c_str();
    auto path_str = dep_path.string();

    // set up options
    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
    checkout_opts.progress_cb = checkout_progress;
    checkout_opts.progress_payload = &sp;
    clone_opts.checkout_opts = checkout_opts;
    clone_opts.fetch_opts.callbacks.sideband_progress = sideband_progress;
    clone_opts.fetch_opts.callbacks.transfer_progress = &fetch_progress;
    // TODO: clone_opts.fetch_opts.callbacks.credentials = cred_acquire_cb;
    clone_opts.fetch_opts.callbacks.payload = &sp;

    // do the clone
    utils::git_init_once(); // make sure libgit2 is initialized
    info("cloning {}", m_expanded);
    int error = git_clone(&cloned_repo, url, path_str.c_str(), &clone_opts);
    if (error != 0) {
        const git_error* err = git_error_last();
        if (err)
            throw std::runtime_error(
                fmt::format("error {}: {}", err->klass, err->message));
        else
            throw std::runtime_error(
                fmt::format("error {}: no detailed info", error));
    } else if (cloned_repo)
        git_repository_free(cloned_repo);
}

void Dependency::fetch_url(const std::filesystem::path& download_path) {
    assert(false && "unimplemented");
}

std::filesystem::path
Dependency::fetch_and_get_path(const std::filesystem::path& deps_dir) {
    auto download_path = deps_dir / (m_name + "-src");

    switch (m_type) {
    case DependencyType::git:
        clone_git_repo(download_path);
        return download_path;
    case DependencyType::url:
        fetch_url(download_path);
        return download_path;
    case DependencyType::path:
        // we don't need to copy or fetch anything, path is already given
        return m_value;
    }
}
