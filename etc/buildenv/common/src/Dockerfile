FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    gdb \
    git \
    less \
    libcurl4-openssl-dev \
    locales \
    ninja-build \
    openssl \
    pkg-config \
    sudo \
    wget \
    zsh \
 && rm -rf /var/lib/apt/lists/*

RUN echo "ALL ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/user \
 && chmod 0440 /etc/sudoers.d/user \
 ;

RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LC_ALL C
