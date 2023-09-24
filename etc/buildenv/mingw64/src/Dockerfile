RUN apt-get update && apt-get install -y --no-install-recommends \
    g++-mingw-w64-x86-64 \
    mingw-w64-x86-64-dev \
    wget \
    zstd \
 && rm -rf /var/lib/apt/lists/*

ADD mingw64/scripts/install_mingw64_package.sh /root

RUN /root/install_mingw64_package.sh libffi-3.4.4-1-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh dlfcn-1.3.1-2-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh gtest-1.13.0-1-any.pkg.tar.zst

RUN apt-get update && apt-get install -y --no-install-recommends \
    wine \
    winetricks \
 && rm -rf /var/lib/apt/lists/*

ENV WINEPATH="/opt/mingw64/bin;/usr/lib/gcc/x86_64-w64-mingw32/9.3-posix/"
ENV WINEDEBUG=-all

RUN /root/install_mingw64_package.sh binutils-2.41-2-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh crt-git-11.0.0.r159.g0605217f5-1-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh gcc-13.2.0-2-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh gcc-libs-13.2.0-2-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh gmp-6.3.0-2-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh headers-git-11.0.0.r159.g0605217f5-1-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh isl-0.26-1-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh libiconv-1.17-3-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh libwinpthread-git-11.0.0.r159.g0605217f5-1-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh mpc-1.3.1-2-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh mpfr-4.2.1-2-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh windows-default-manifest-6.4-4-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh winpthreads-git-11.0.0.r159.g0605217f5-1-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh zlib-1.3-1-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh zstd-1.5.5-1-any.pkg.tar.zst

RUN /root/install_mingw64_package.sh json-c-0.17-1-any.pkg.tar.zst

RUN rm /root/install_mingw64_package.sh
