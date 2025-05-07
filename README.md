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
- [Building from source](#building-from-source)
    - [Install build dependencies](#install-build-dependencies)
    - [Build](#build)
- [Building from source (with docker)](#building-from-source-with-docker)
    - [Install build dependencies](#install-build-dependencies-1)
    - [Start container runtime](#start-container-runtime)
    - [Build](#build-1)

# Building from source

## Install build dependencies

### Ubuntu

```sh
sudo apt install build-essential pkg-config cmake libffi-dev

# Optional:
sudo apt install ninja-build ccache

# With tests:
sudo apt install libgtest-dev
```

### Arch

```sh
sudo pacman -S gcc make pkgconf cmake libffi

# Optional:
sudo pacman -S ninja ccache

# With tests:
sudo pacman -S gtest perl diffutils
```

### Darwin

```sh
brew install pkgconf cmake

# Optional:
brew install ninja ccache

# With tests:
brew install googletest
```

## Build

For native build use:
```sh
./build -n
```

# Building from source (with docker)

Nickl build scripts are using docker containers as a build environment by default.

Currently following target platforms are supported:
- Linux
- Windows (Mingw-w64)
- Darwin

> [!NOTE]
> Darwin build with docker must only be done on Apple hardware, because it requires an Apple SDK.
> [Please read Xcode license agreement.](https://www.apple.com/legal/sla/docs/xcode.pdf)

## Install build dependencies

### Ubuntu

```sh
sudo apt install docker.io jq
```

### Arch

```sh
sudo pacman -S docker jq
```

### Darwin

```sh
brew install docker colima jq
```

## Start container runtime

### Linux

```sh
sudo systemctl start docker.service
```

Don't forget to add your user to the `docker` group.
```sh
sudo usermod -aG docker $USER
newgrp docker # or relogin
```

### Darwin

```sh
colima start
```

## Build

Build script will automatically pull or build the necessary docker image.

### Linux

```sh
./build.sh
```

### Darwin

```sh
./build.sh -k /path/to/dir/with/sdk
```
