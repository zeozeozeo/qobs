#include "builder.hpp"
#include "config.hpp"
#include "spdlog/spdlog.h"
#include "utils.hpp"
#include <argparse/argparse.hpp>
#include <filesystem>
#include <fmt/core.h>
#include <iostream>
#include <optional>

#include "generators/ninja/ninja_gen.hpp"

using namespace spdlog;

constexpr auto& CONFIG_NAME = "Qobs.toml";
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
    if (!path.is_absolute()) {
        trace("path `{}` is relative, promoting to absolute", path.string());
        path = std::filesystem::absolute(path);
    }
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
    // packages, and generate the project
    Builder builder(config);
    try {
        builder.build();
    } catch (const std::exception& err) {
        error("failed to build package: {}", err.what());
        return;
    }

    // create a generator
    NinjaGenerator generator(builder);
    generator.generate();

    // print the ninja code
    auto ninja_path = builder.config().package_path() / "build" / "build.ninja";
    std::fstream file(ninja_path, std::ios::out);
    file << generator.code();
    file.close();

    info("generated project file:\n{}", generator.code());
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
    Config config{path};
    config.m_package.m_name = name;

    // description
    fmt::print("Description (optional): ");
    std::getline(std::cin, config.m_package.m_description);
    utils::trim_in_place(config.m_package.m_description);

    // authors
    fmt::print("Author (optional): ");
    std::string author;
    std::getline(std::cin, author);
    utils::trim_in_place(author);
    if (!author.empty()) {
        config.m_package.add_author(author);
    }

    // use C++ (y/n)?
    fmt::print("Use C++ (y/n)? ");
    std::string use_cpp;
    std::getline(std::cin, use_cpp);
    utils::trim_in_place(use_cpp);
    bool cxx =
        use_cpp == "y" || use_cpp == "Y" || use_cpp == "1" || use_cpp.empty();

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

    auto config_path = path / CONFIG_NAME;
    try {
        config.save_to(config_path);
    } catch (const std::exception& err) {
        error("couldn't create `{}`: {}", config_path.string(), err.what());
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
        try {
            begin_build(path);
        } catch (const std::exception& err) {
            error("failed to begin build: {}", err.what());
            return 1;
        }
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
