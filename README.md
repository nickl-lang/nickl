<p align="center">
    <img src="etc/images/nkl.svg" alt="nickl">
</p>

[![Nickl CI](https://github.com/nickl-lang/nickl/actions/workflows/ci.yml/badge.svg)](https://github.com/nickl-lang/nickl/actions/workflows/ci.yml)

# The Nickl Programming Language

> [!WARNING]
> This project is in the research/prototype phase. Expect chaos!

Nickl is a compiled statically typed programming language that is a part of a research project
focused on exploring the boundaries of what happens when a compiler exposes a rich API
to the program with ability to execute arbitrary code at compile time.

# Table of Contents
- [Dependencies](#dependencies)
- [Build from source](#build-from-source)
    - [Install dependencies](#install-dependencies)
      - [Ubuntu](#ubuntu)
      - [Arch](#arch)
      - [Darwin](#darwin)
      - [MSYS2](#msys2)
      - [Windows](#windows)
    - [Build](#build)

# Dependencies

 - LLVM (15+)
 - libffi

# Build from source

## Install dependencies

### Ubuntu

```sh
sudo apt install \
  build-essential \
  cmake \
  libffi-dev \
  llvm \
  pkg-config \
  zlib1g-dev \
  ;

# Optional:
sudo apt install \
  ccache \
  ninja-build \
  ;

# For tests:
sudo apt install \
  libgtest-dev \
  ;
```

### Arch

```sh
sudo pacman -S \
  cmake \
  gcc \
  libffi \
  llvm \
  make \
  pkgconf \
  ;

# Optional:
sudo pacman -S \
  ccache \
  ninja \
  ;

# For tests:
sudo pacman -S \
  diffutils \
  gtest \
  perl \
  ;
```

### Darwin

```sh
brew install \
  cmake \
  llvm \
  pkgconf \
  ;

# Optional:
brew install \
  ccache \
  ninja \
  ;

# For tests:
brew install \
  googletest \
  ;
```

### MSYS2

> [!TODO]

### Windows

> [!TODO]

## Build

```sh
./build.sh
```

See help for more options:
```sh
./build.sh -h
```
