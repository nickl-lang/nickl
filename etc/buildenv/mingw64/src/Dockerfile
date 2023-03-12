RUN apt-get update && apt-get install -y --no-install-recommends \
    g++-mingw-w64-x86-64 \
    mingw-w64-x86-64-dev \
    zstd \
 && rm -rf /var/lib/apt/lists/*

ADD mingw64/scripts/install_mingw64_package.sh /root

RUN /root/install_mingw64_package.sh libffi-3.4.4-1-any.pkg.tar.zst
RUN /root/install_mingw64_package.sh dlfcn-1.3.1-2-any.pkg.tar.zst

RUN rm /root/install_mingw64_package.sh
