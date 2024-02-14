LABEL org.opencontainers.image.description="Windows build environment for Nickl compiler"

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++-mingw-w64-x86-64 \
    mingw-w64-x86-64-dev \
    wine \
    winetricks \
    #zstd \
 && rm -rf /var/lib/apt/lists/*

ENV WINEPATH="/opt/mingw64/bin;/usr/lib/gcc/x86_64-w64-mingw32/9.3-posix/"
ENV WINEDEBUG=-all

RUN apt-get update && apt-get install -y --no-install-recommends \
    libarchive-dev \
    libgpgme-dev \
    libssl-dev \
    python3-pip \
 && python3 -m pip --no-cache-dir install meson \
 && export CCACHE_DISABLE=1 \
 && git clone --depth=1 --branch=v6.0.0 https://gitlab.archlinux.org/pacman/pacman.git /root/pacman \
 && cd /root/pacman \
 && meson build \
 && ninja -C build \
 && ninja -C build install \
 && mkdir -p /usr/share/doc/pacman/ \
 && cp COPYING README /usr/share/doc/pacman/ \
 && python3 -m pip uninstall -y meson \
 && apt remove -y python3-pip \
 && rm -rf /root/pacman \
 && rm -rf /var/lib/apt/lists/* \
 ;

RUN git clone https://github.com/msys2/MSYS2-keyring /root/MSYS2-keyring \
 && cd /root/MSYS2-keyring \
 && mkdir -p /usr/share/pacman/keyrings/ \
 && cp msys2.gpg msys2-trusted msys2-revoked /usr/share/pacman/keyrings/ \
 && rm -rf /root/MSYS2-keyring \
 ;

RUN pacman-key --init \
 && pacman-key --populate msys2 \
 ;

ADD mingw64/src/pacman.conf /etc/
ADD mingw64/src/mirrorlist.mingw64 /etc/pacman.d/

RUN ln -sf /proc/self/mounts /etc/mtab \
 && pacman -Syu --noconfirm \
    mingw-w64-x86_64-dlfcn \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-gtest \
    mingw-w64-x86_64-json-c \
    mingw-w64-x86_64-libffi \
 && rm -rf /var/cache/pacman/pkg/* \
 && rm -rf /var/lib/pacman/sync/* \
 && rm /etc/mtab \
 ;

ADD common/scripts/install_spall.sh /root/
RUN /root/install_spall.sh /opt/mingw64
RUN rm /root/install_spall.sh
