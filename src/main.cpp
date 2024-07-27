#include "builder.hpp"
#include "config.hpp"
#include "spdlog/spdlog.h"
#include <argparse/argparse.hpp>
#include <filesystem>
#include <fmt/core.h>
#include <iostream>
#include <optional>

using namespace spdlog;

const std::string CONFIG_NAME = "Qobs.toml";

// Try to find Qobs.toml in current directory or in parent directories
std::optional<std::filesystem::path>
find_qobs_toml(std::filesystem::path initial_path) {
    auto path = initial_path;
    auto root = initial_path.root_path();
    while (!std::filesystem::exists(path / CONFIG_NAME)) {
        trace("searching for Qobs.toml inside `{}`", path.string());
        if (path == root) {
            return std::nullopt;
        }
        path = path.parent_path();
    }
    return path / CONFIG_NAME;
}

void begin_build(std::filesystem::path path) {
    debug("building package: {}", path.string());

    // find Qobs.toml
    auto qobs_toml_path = find_qobs_toml(path);
    if (!qobs_toml_path) {
        error("{} not found in `{}` or any parent directory", CONFIG_NAME,
              path.string());
        return;
    }
    auto toml_path = *qobs_toml_path;

    // parse Qobs.toml
    Config config{toml_path.parent_path()};
    try {
        config.parse_file(toml_path.string());
    } catch (const std::exception& err) {
        error("couldn't parse `{}`: {}", toml_path.string(), err.what());
        return;
    }

    // create builder & build package
    Builder builder(config);
    builder.build();
}

int main(int argc, char* argv[]) {
    set_pattern("%^%l%$: %v");
    set_level(level::trace);
    argparse::ArgumentParser program("qobs");

    // qobs build
    argparse::ArgumentParser build_command("build");
    build_command.add_description("Compile a package");
    build_command.add_argument("path")
        .help("Path to the package to build")
        .default_value(std::filesystem::current_path().string())
        .nargs(argparse::nargs_pattern::optional);

    // add subparsers
    program.add_subparser(build_command); // qobs build

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        error("failed to parse arguments: {}", err.what());
        return 1;
    }

    // print help if no arguments were provided
    if (argc == 1) {
        std::cout << program;
        return 0;
    }

    // handle "qobs build"
    if (program.is_subcommand_used("build")) {
        auto path = build_command.get<std::string>("path");
        begin_build(path);
        return 0;
    }

    return 0;
}
