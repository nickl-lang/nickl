FROM alpine:edge

RUN apk add --no-cache \
    ca-certificates \
    clang15-extra-tools \
    cmake \
    curl \
    gdb \
    git \
    less \
    libcurl \
    ninja \
    openssh \
    openssl \
    pkgconf \
    sudo \
    tmux \
    wget \
    zsh \
    ;
# TODO
    # locales \
    # build-essential \

RUN echo "ALL ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/user \
 && chmod 0440 /etc/sudoers.d/user \
 ;

# TODO
# RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LC_ALL C

RUN mkdir -p -m 0600 ~/.ssh \
 && ssh-keyscan github.com > ~/.ssh/known_hosts \
 ;
