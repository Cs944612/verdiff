# Verdiff

`verdiff` is a high-performance C directory comparison engine for large trees and codebases.

[![CI](https://github.com/Cs944612/verdiff/actions/workflows/ci.yml/badge.svg)](https://github.com/Cs944612/verdiff/actions/workflows/ci.yml)

Verdiff is designed for fast, accurate comparison of large directory trees with a metadata-first pipeline, bounded parallelism, and detailed diff reporting when needed.

## Features

- Iterative directory scanning with explicit stacks
- Custom arena-backed hashmap index for metadata-first planning
- Custom AVL path tree for ordered reporting
- Bounded pthread worker pool for safe parallel comparison
- Real upstream `xxHash` integration with `XXH3`
- `mmap` fast path for smaller files and streaming hashing for larger files
- Byte-for-byte verification after hash equality for reliability
- Clean CLI summary plus file-based detailed report
- Linux-ready `make`, `make test`, and `make install`
- Public-repo friendly structure with CI and contribution docs

## Why Verdiff

- Fast unchanged detection through metadata and exact compare short-circuiting
- Accurate modified-file detection without sacrificing deterministic behavior
- Built for large codebases, CI jobs, and backup verification workflows
- Simple to build on Linux without external runtime dependencies

## Build

```sh
make
```

After `make`, run it directly from the repository root:

```sh
./verdiff /path/to/source /path/to/target
./verdiff --thread 8 --lines /path/to/source /path/to/target
```

## Test

```sh
make test
```

## Benchmark

```sh
make bench
```

Comparative benchmark against standard tools on a real local tree:

```sh
make bench-compare
```

You can also control the generated dataset size:

```sh
sh bench/run_benchmark.sh 20000
```

## Install

```sh
sudo make install PREFIX=/usr/local
```

Run from anywhere after install:

```sh
verdiff --thread 8 --lines /path/to/source /path/to/target
```

To remove the installed binary and docs:

```sh
sudo make uninstall PREFIX=/usr/local
```

## Usage

```sh
./verdiff --thread 8 --lines DIR_A DIR_B
./verdiff DIR_A DIR_B
./verdiff --help
./verdiff --version
```

Stdout prints only the summary.

The detailed report is written to:

- `verdiff_report.txt` by default
- the file passed with `-o <path>` when provided

## Project Layout

- `src/`: core engine
- `include/`: public internal headers
- `third_party/xxhash/`: vendored upstream `xxHash`
- `tests/`: smoke coverage
- `bench/`: repeatable correctness and speed benchmark
- `build/bin/`: compiled executable
- `build/obj/`: object files

## Open Source Project Setup

- License: MIT
- CI: GitHub Actions
- Contribution guide: [CONTRIBUTING.md](./CONTRIBUTING.md)
- Security policy: [SECURITY.md](./SECURITY.md)
- Code of conduct: [CODE_OF_CONDUCT.md](./CODE_OF_CONDUCT.md)

## Exit Codes

- `0`: success
- `1`: runtime error
- `2`: usage error
