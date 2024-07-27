# Qobs, the Quite OK Build System

C/C++'s missing package manager.

Qobs is similar to source code managers such as Cargo and Go: it features a complete package manager combined with a build system.

This file is a complete guide on using Qobs.

# Quickstart

Let's create our first Qobs project. Fire up your terminal and run the following command:
```bash
$ qobs new my-first-qobs-project
```

After filling out a few prompts, Qobs will scaffold a directory called `my-first-qobs-project`. Let's `cd` inside it and see what we've got:

```
my-first-qobs-project/
├── src/
│   └── main.cpp
└── Qobs.toml
```

Qobs has created a directory with an `src` folder with a `main.cpp` (or `main.c`) file for us. It has also created a file called `Qobs.toml`. Let's see what's inside it:

# Qobs.toml

Qobs uses [TOML](https://toml.io) for its' configuration files. The syntax is quite similar to Cargo, so if you're a Rust developer, this will look familiar to you.

Let's look inside the `Qobs.toml` file that Qobs has made for us:

```toml
[package]
name = "my-first-qobs-project"
description = "This is where I make a project."
authors = ["AzureDiamond"]
sources = ["src/*.cpp", "src/*.cc", "src/*.c"]

[dependencies]
```

This is pretty simple, however this file is very customizable.

### `[package]`

General package info, such as name, description, authors and source location.

`package.name` (string, required): name of the package, the output executable, PDB and object file.

`package.description` (string): optional package description

`package.authors` (array of strings): package authors (optional)

`package.sources` (array of strings): list of globs for package source files. This is optional and defaults to `["src/*.cpp", "src/*.cc", "src/*.c"]`

#### `package.sources` wildcard format

Qobs uses [p-ranav/glob](https://github.com/p-ranav/glob) for globbing files, which supports the following wildcards:

| Wildcard | Matches                                       | Example                                                 |
| -------- | --------------------------------------------- | ------------------------------------------------------- |
| `*`      | any characters                                | `*.txt` matches all files with the txt extension        |
| `?`      | any one character                             | `???` matches files with 3 characters long              |
| `[]`     | any character listed in the brackets          | `[ABC]*` matches files starting with A,B or C           |
| `[-]`    | any character in the range listed in brackets | `[A-Z]*` matches files starting with capital letters    |
| `[!]`    | any character not listed in the brackets      | `[!ABC]*` matches files that do not start with A,B or C |

# Generators

Qobs does not build your code by itself, it instead generates project files for other build systems such as [Ninja](https://ninja-build.org/).

# Bootstrapping

Qobs uses CMake to bootstrap itself, required dependencies are pulled with [CPM](https://github.com/cpm-cmake/CPM.cmake). After building Qobs with CMake, you should be able to use the compiled executable to configure and compile Qobs with itself!
