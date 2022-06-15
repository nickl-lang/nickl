FROM debian:sid-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    clang \
    clang-format \
    cmake \
    curl \
    gdb \
    git \
    less \
    libcurl4-openssl-dev \
    locales \
    ninja-build \
    openssl \
    pkg-config \
    python-is-python3 \
    python3-dev \
    ssh \
    sudo \
    tmux \
    wget \
    zsh \
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

RUN apt-get update && apt-get install -y --no-install-recommends \
    libgtest-dev libgmock-dev libffi-dev \
 && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3-pip python3-neovim nodejs npm clangd \
 && rm -rf /var/lib/apt/lists/*

RUN npm install -g neovim

RUN wget https://github.com/neovim/neovim/releases/download/v0.7.0/nvim-linux64.deb \
 && apt-get install ./nvim-linux64.deb \
 && rm ./nvim-linux64.deb \
 ;
