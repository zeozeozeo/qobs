#include "builder.hpp"
#include "manifest.hpp"
#include "spdlog/spdlog.h"
#include "utils.hpp"
#include <argparse/argparse.hpp>
#include <filesystem>
#include <fmt/core.h>
#include <iostream>
#include <optional>

#include "generators/ninja/ninja_gen.hpp"

using namespace spdlog;

constexpr auto& MANIFEST_NAME = "Qobs.toml";
constexpr auto& DEFAULT_C = R"(#include <stdio.h>

int main(void) {
    printf("Hello, World!");
    return 0;
}
)";

constexpr auto& DEFAULT_CPP = R"(#include <iostream>

int main() {
    std::cout << "Hello, World!\n";
    return 0;
}
)";

// Try to find Qobs.toml in current directory or in parent directories
std::optional<std::filesystem::path>
find_qobs_toml(std::filesystem::path initial_path) {
    auto path = initial_path;
    auto root = initial_path.root_path();
    while (!std::filesystem::exists(path / MANIFEST_NAME)) {
        trace("searching for Qobs.toml inside `{}`", path.string());
        if (path == root) {
            return std::nullopt;
        }
        path = path.parent_path();
    }
    trace("found Qobs.toml in `{}`", path.string());
    return path / MANIFEST_NAME;
}

std::optional<std::pair<Manifest, std::filesystem::path>>
find_and_parse_manifest(std::filesystem::path path) {
    if (!path.is_absolute()) {
        path = std::filesystem::absolute(path);
    }

    auto toml_path = find_qobs_toml(path);
    if (!toml_path) {
        error("{} not found in `{}` or any parent directory", MANIFEST_NAME,
              path.string());
        return std::nullopt;
    }

    // parse Qobs.toml
    Manifest manifest{toml_path->parent_path()};
    try {
        manifest.parse_file(toml_path->string());
    } catch (const std::exception& err) {
        error("couldn't parse `{}`: {}", toml_path->string(), err.what());
        return std::nullopt;
    }

    return std::make_pair(manifest, *toml_path);
}

// returns path to the built executable/library
std::optional<std::filesystem::path>
begin_build(std::filesystem::path path, std::string_view build_dir,
            std::optional<std::string> cc) {
    debug("building package: {}", path.string());

    auto manifest_opt = find_and_parse_manifest(path);
    if (!manifest_opt)
        return std::nullopt;
    auto [manifest, _] = *manifest_opt;

    // create a generator
    auto gen = std::make_shared<NinjaGenerator>();

    // create builder, this will scan the package sources, download required
    // packages, and generate the project
    Builder builder(manifest);
    try {
        return builder.build(gen, build_dir, cc);
    } catch (const std::exception& err) {
        error("failed to build package: {}", err.what());
        return std::nullopt;
    }
}

void new_package(std::string name) {
    while (name.empty()) {
        fmt::print("Package name: ");
        std::getline(std::cin, name);
        utils::trim_in_place(name);
        if (name.empty()) {
            error("package name cannot be empty");
        }
    }

    auto path = std::filesystem::current_path() / name;
    if (std::filesystem::exists(path)) {
        error("couldn't create package `{}`: directory `{}` already exists",
              name, path.string());
        return;
    }

    // create Qobs.toml
    Manifest manifest{path};
    manifest.m_package.m_name = name;

    // description
    fmt::print("Description (optional): ");
    std::getline(std::cin, manifest.m_package.m_description);
    utils::trim_in_place(manifest.m_package.m_description);

    // authors
    fmt::print("Author (optional): ");
    std::string author;
    std::getline(std::cin, author);
    utils::trim_in_place(author);
    if (!author.empty()) {
        manifest.m_package.add_author(author);
    }

    // use C++ (y/n)?
    fmt::print("Use C++ (y/n)? ");
    std::string use_cpp;
    std::getline(std::cin, use_cpp);
    utils::trim_in_place(use_cpp);
    bool cxx =
        use_cpp == "y" || use_cpp == "Y" || use_cpp == "1" || use_cpp.empty();
    manifest.m_target.m_cxx = cxx;

    // scaffold package directory
    auto scaffold_path = path;
    try {
        std::filesystem::create_directory(scaffold_path);
        scaffold_path /= "src";
        std::filesystem::create_directory(scaffold_path);
    } catch (const std::exception& err) {
        error("couldn't create directory `{}`: {}", scaffold_path.string(),
              err.what());
        return;
    }

    auto manifest_path = path / MANIFEST_NAME;
    try {
        manifest.save_to(manifest_path);
    } catch (const std::exception& err) {
        error("couldn't create `{}`: {}", manifest_path.string(), err.what());
        return;
    }

    // create src/main.c or src/main.cpp
    scaffold_path /= (cxx ? "main.cpp" : "main.c");
    try {
        std::ofstream file(scaffold_path, std::ios::out | std::ios::trunc);
        if (cxx) {
            file << DEFAULT_CPP;
        } else {
            file << DEFAULT_C;
        }
        file.close();
    } catch (const std::exception& err) {
        error("couldn't create `{}`: {}", scaffold_path.string(), err.what());
        return;
    }

    info("created {} package `{}` in `{}`", cxx ? "C++" : "C", name,
         path.string());
}

void add_dependencies(std::filesystem::path path,
                      std::vector<std::string> deps) {
    auto manifest_opt = find_and_parse_manifest(path);
    if (!manifest_opt)
        return; // error printed in find_and_parse_manifest
    auto [manifest, toml_path] = *manifest_opt;

    for (auto& dep : deps) {
        if (dep.empty())
            continue;

        std::string name;
        for (;;) {
            fmt::print("Dependency name for `{}`? ", dep);
            std::getline(std::cin, name);
            if (name.empty())
                error("dependency name cannot be empty");
            else if (manifest.m_dependencies.has(name, dep))
                error("dependency `{} = '{}'` already exists", name, dep);
            else
                break;
        }

        manifest.m_dependencies.add({name, dep});
    }

    try {
        manifest.save_to(toml_path);
    } catch (const std::exception& err) {
        error("couldn't update `{}`: {}", toml_path.string(), err.what());
    }
}

void validate_build_dir(std::string& build_dir) {
    if (!utils::is_directory_valid(build_dir)) {
        warn("invalid build directory `{}`, defaulting to `build`", build_dir);
        build_dir = "build";
    }
}

level::level_enum get_level_from_name(std::string_view name) {
    if (name == "trace")
        return level::trace;
    if (name == "debug")
        return level::debug;
    if (name == "info")
        return level::info;
    if (name == "warn")
        return level::warn;
    if (name == "error")
        return level::err;
    if (name == "critical")
        return level::critical;
    if (name == "off")
        return level::off;
    return level::info;
}

void atexit_handler() {
    // libgit2 bookkeeping
    utils::maybe_shutdown_git();
}

int main(int argc, char* argv[]) {
    std::atexit(atexit_handler);
    set_pattern("%^%l%$: %v"); // `info: abcd` where `info` is colored green
    argparse::ArgumentParser program("qobs");

    program.add_argument("-l", "--log-level")
        .default_value("info")
        .choices("trace", "debug", "info", "warn", "error", "critical", "off");

    // qobs new
    argparse::ArgumentParser new_command("new");
    new_command.add_description("Create a new package");
    new_command.add_argument("name").implicit_value("").help(
        "Name of the package");

    program.add_subparser(new_command);

    auto current_path = std::filesystem::current_path().string();

    // qobs build
    argparse::ArgumentParser build_command("build");
    build_command.add_description("Compile a package");
    build_command.add_argument("path")
        .help("Path to the package to build")
        .default_value(current_path);
    build_command.add_argument("-cc").help(
        "Override the default C/C++ compiler");
    build_command.add_argument("-b", "--build-dir")
        .default_value("build")
        .help("Build directory");

    // qobs run
    argparse::ArgumentParser run_command("run");
    run_command.add_description("Compile and run a package");
    run_command.add_argument("path")
        .help("Path to the package to run")
        .default_value(current_path);
    run_command.add_argument("-cc").help("Override the default C/C++ compiler");
    run_command.add_argument("-b", "--build-dir")
        .default_value("build")
        .help("Build directory");
    // FIXME: in the --help message for this, it is displayed like this:
    // run [--help] [--version] [-cc VAR] [--build-dir VAR] [-- VAR...] path
    //                                                      ^^^^^^^^^^^
    // this should be printed **after** the `path` argument, not before it
    // this seems to be an argparse issue
    run_command.add_argument("--")
        .help("All arguments after this will be passed to the program")
        .nargs(argparse::nargs_pattern::any);

    // qobs add
    argparse::ArgumentParser add_command("add");
    add_command.add_description("Add dependencies to a manifest file");
    add_command.add_argument("-p", "--path")
        .help("Path to the package")
        .default_value(current_path);
    // FIXME: this shows as [nargs: 0 or more] in the --help message,
    // seems to be an argparse issue
    add_command.add_argument("deps")
        .nargs(argparse::nargs_pattern::at_least_one)
        .remaining();

    // add subparsers
    program.add_subparser(new_command);   // qobs new
    program.add_subparser(build_command); // qobs build
    program.add_subparser(run_command);   // qobs run
    program.add_subparser(add_command);   // qobs add

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        error(err.what());
        return 1;
    }

    // print help if no arguments were provided
    if (argc == 1) {
        std::cout << program;
        return 0;
    }

    // set logger level
    set_level(get_level_from_name(program.get<std::string>("--log-level")));

    // handle subcommands
    if (program.is_subcommand_used("build")) {
        auto path = build_command.get<std::string>("path");
        auto build_dir = build_command.get<std::string>("--build-dir");
        validate_build_dir(build_dir);
        auto cc = build_command.present<std::string>("-cc");

        std::optional<std::filesystem::path> exe_path;
        try {
            exe_path = begin_build(path, build_dir, cc);
        } catch (const std::exception& err) {
            error("failed to begin build: {}", err.what());
            return 1;
        }
        return exe_path ? 0 : 1;
    } else if (program.is_subcommand_used("new")) {
        std::string name;
        if (new_command.is_used("name")) {
            name = new_command.get<std::string>("name");
        }
        new_package(name);
    } else if (program.is_subcommand_used("run")) {
        // almost the same as build
        auto path = run_command.get<std::string>("path");
        auto build_dir = run_command.get<std::string>("--build-dir");
        validate_build_dir(build_dir);
        auto cc = build_command.present<std::string>("-cc");

        std::optional<std::filesystem::path> exe_path;
        try {
            exe_path = begin_build(path, build_dir, cc);
        } catch (const std::exception& err) {
            error("failed to begin build: {}", err.what());
            return 1;
        }

        if (!exe_path)
            return 1;

        // all arguments after -- are passed to the program
        std::vector<std::string> args;
        if (run_command.is_used("--")) {
            args = run_command.get<std::vector<std::string>>("--");
        }

        // on Unix, the path needs to be prefixed with ./
        // TODO: make this use `utils::popen`
#ifdef QOBS_IS_WINDOWS
        auto cmd =
            fmt::format("\"{}\" {}", exe_path->string(), fmt::join(args, " "));
#else
        auto cmd =
            fmt::format("\".{}\" {}", exe_path->string(), fmt::join(args, " "));
#endif

        trace(cmd);
        system(cmd.c_str());
    } else if (program.is_subcommand_used("add")) {
        auto path = add_command.get<std::string>("--path");
        if (add_command.is_used("deps")) {
            auto deps = add_command.get<std::vector<std::string>>("deps");
            add_dependencies(path, deps);
        } else {
            error("no dependencies provided. use `qobs add -h` for help");
            return 1;
        }
    }

    return 0;
}
