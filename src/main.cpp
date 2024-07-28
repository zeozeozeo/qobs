#include "builder.hpp"
#include "config.hpp"
#include "spdlog/spdlog.h"
#include <argparse/argparse.hpp>
#include <filesystem>
#include <fmt/core.h>
#include <iostream>
#include <optional>

#include "generators/ninja/ninja_gen.hpp"

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

    // create builder, this will scan the package sources, download required
    // packages, etc. this will be passed to the generator, which will create
    // the actual project files.
    Builder builder(config);
    builder.build();

    // create a generator
    NinjaGenerator generator(builder);
    generator.generate();

    // print the ninja code
    info("generated project file:\n{}", generator.code());
}

void new_package(std::string name) {
    while (name.empty()) {
        fmt::print("Package name: ");
        std::getline(std::cin, name);
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

    // create package directory
    try {
        std::filesystem::create_directory(path);
    } catch (const std::exception& err) {
        error("couldn't create directory `{}`: {}", path.string(), err.what());
        return;
    }

    // create Qobs.toml
    Config config{path};
    config.m_package.m_name = name;
    auto config_path = path / CONFIG_NAME;
    try {
        config.save_to(config_path);
    } catch (const std::exception& err) {
        error("couldn't create `{}`: {}", config_path.string(), err.what());
        return;
    }

    info("created package `{}`", name);
}

int main(int argc, char* argv[]) {
    set_pattern("%^%l%$: %v");
    set_level(level::trace);
    argparse::ArgumentParser program("qobs");

    // qobs new
    argparse::ArgumentParser new_command("new");
    new_command.add_description("Create a new package");
    new_command.add_argument("name").implicit_value("").help(
        "Name of the package");

    program.add_subparser(new_command);

    // qobs build
    argparse::ArgumentParser build_command("build");
    build_command.add_description("Compile a package");
    build_command.add_argument("path")
        .help("Path to the package to build")
        .default_value(std::filesystem::current_path().string())
        .nargs(argparse::nargs_pattern::optional);

    // add subparsers
    program.add_subparser(build_command); // qobs build
    program.add_subparser(new_command);   // qobs new

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

    // handle subcommands
    if (program.is_subcommand_used("build")) {
        auto path = build_command.get<std::string>("path");
        begin_build(path);
        return 0;
    } else if (program.is_subcommand_used("new")) {
        std::string name;
        if (new_command.is_used("name")) {
            name = new_command.get<std::string>("name");
        }
        new_package(name);
        return 0;
    }

    return 0;
}
