FROM debian:sid-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
# Essentials \
    build-essential \
    ca-certificates \
    curl \
    libcurl4-openssl-dev \
    locales \
    openssl \
    pkg-config \
    python-is-python3 \
    python3-dev \
    ssh \
    sudo \
    tmux \
    wget \
    zsh \
# Build tools \
    clang \
    clang-format \
    cmake \
    gdb \
    git \
    ninja-build \
# Dev Tools \
    clangd \
    less \
    nodejs \
    npm  \
    python3-neovim \
    python3-pip \
    silversearcher-ag \
# Project dependencies \
    libffi-dev \
    libgmock-dev \
    libgtest-dev \
 && rm -rf /var/lib/apt/lists/*

RUN echo "ALL ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/user \
 && chmod 0440 /etc/sudoers.d/user \
 ;

RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LC_ALL C

RUN mkdir -p -m 0600 ~/.ssh \
 && ssh-keyscan github.com > ~/.ssh/known_hosts \
 ;

RUN git clone https://github.com/yse/easy_profiler.git \
 && mkdir easy_profiler/build \
 && cd easy_profiler/build \
 && cmake -GNinja .. \
 && ninja install \
 && cd ../.. \
 && rm -rf easy_profiler \
 ;

RUN npm install -g neovim

RUN wget https://github.com/neovim/neovim/releases/download/v0.7.2/nvim-linux64.deb \
 && apt-get install ./nvim-linux64.deb \
 && rm ./nvim-linux64.deb \
 ;
