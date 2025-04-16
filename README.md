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

# Building from source (with docker)

Nickl build scripts are using docker containers as a build environment by default.

Currently following platforms are supported:
- Linux
- Windows (Mingw-w64)
- Darwin

> [!NOTE]
> Darwin build with docker must only be done on Apple hardware, because it requires an Apple SDK.
> [Please read Xcode license agreement.](https://www.apple.com/legal/sla/docs/xcode.pdf)

## Build dependencies

- Ubuntu

```
# apt install docker.io jq
```

- Arch

```
# pacman -S docker jq
```

Don't forget to add your user to the `docker` group.
```
# usermod -aG docker $USER
$ newgrp docker # or relogin
```

## Build

Build script will automatically pull the necessary docker container.

```
$ ./build.sh
```

# Building from source (native)

## Build dependencies

- Ubuntu

```
# apt install build-essential pkg-config cmake libffi-dev

## Optional:
# apt install ninja-build ccache

## Testing:
# apt install libgtest-dev
```

- Arch

```
# pacman -S gcc make pkgconf cmake libffi

## Optional:
# pacman -S ninja ccache

## Testing:
# pacman -S gtest perl diffutils
```

## Build

For native build use
```
./build -n
```
