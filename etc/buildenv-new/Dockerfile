### base #######################################################################

FROM ubuntu:20.04 AS base

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    ccache \
    curl \
    gdb \
    git \
    less \
    locales \
    ninja-build \
    openssl \
    pkg-config \
    software-properties-common \
    sudo \
    wget \
    zsh \
 && rm -rf /var/lib/apt/lists/*

RUN echo "ALL ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/user \
 && chmod 0440 /etc/sudoers.d/user \
 && echo 'account sufficient pam_permit.so' > /etc/pam.d/sudo \
 && echo 'auth    sufficient pam_permit.so' >> /etc/pam.d/sudo \
 ;

RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LC_ALL=C

### cmake ######################################################################

FROM base AS cmake

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl-dev \
 && rm -rf /var/lib/apt/lists/*

ARG CMAKE_VERSION=3.31.6
RUN cd /root \
 && wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION.tar.gz \
 && tar xzf cmake-$CMAKE_VERSION.tar.gz \
 && cd cmake-$CMAKE_VERSION \
 && ./bootstrap \
      --parallel=$(nproc) \
      --prefix=/opt/cmake \
      -- \
      -DBUILD_TESTING=OFF \
      -DCMAKE_BUILD_TYPE=Release \
 && make -j \
 && make install \
 && rm -rf /root/* \
 ;

### common #####################################################################

FROM base AS common

COPY --from=cmake /opt/cmake/ /usr/local/

### toolchain-common ###########################################################

FROM common AS toolchain-common

ARG BINUTILS_VERSION=2.44
RUN cd /root \
 && wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz \
 && tar xvf binutils-$BINUTILS_VERSION.tar.xz \
 && rm binutils-$BINUTILS_VERSION.tar.xz \
 ;

ARG GCC_VERSION=14.2.0
RUN cd /root \
 && wget http://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz \
 && tar xvf gcc-$GCC_VERSION.tar.gz \
 && rm gcc-$GCC_VERSION.tar.gz \
 ;

RUN cd /root/gcc-$GCC_VERSION \
 && ./contrib/download_prerequisites \
 ;

### toolchain-linux ############################################################

FROM toolchain-common AS toolchain-linux

RUN cd /root/binutils-$BINUTILS_VERSION \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --prefix=/opt \
      --disable-multilib \
 && make -j \
 && make install-strip \
 && rm -rf /root/binutils-gdb \
 ;

ENV PATH="/opt/bin/:$PATH"

ARG M4_VERSION=1.4.19
RUN cd /root \
 && wget https://ftp.gnu.org/gnu/m4/m4-$M4_VERSION.tar.gz \
 && tar xvf m4-$M4_VERSION.tar.gz \
 && cd m4-$M4_VERSION \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --prefix=/usr/local \
 && make -j \
 && make install-strip \
 && rm -rf /root/m4-$M4_VERSION \
 ;

RUN cd /root/gcc-$GCC_VERSION/gmp \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --prefix=/opt \
      --enable-shared \
 && make -j \
 && make install-strip \
 && make distclean \
 ;

RUN cd /root/gcc-$GCC_VERSION/mpfr \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --prefix=/opt \
      --with-gmp=/opt \
      --enable-shared \
 && make -j \
 && make install-strip \
 && make distclean \
 ;

RUN cd /root/gcc-$GCC_VERSION/mpc \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --prefix=/opt \
      --with-gmp=/opt \
      --with-mpfr=/opt \
      --enable-shared \
 && make -j \
 && make install-strip \
 && make distclean \
 ;

ARG GDB_VERSION=16.2
RUN cd /root/ \
 && wget https://ftp.gnu.org/gnu/gdb/gdb-$GDB_VERSION.tar.xz \
 && tar xvf gdb-$GDB_VERSION.tar.xz \
 && rm gdb-$GDB_VERSION.tar.xz \
 ;

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3-dev \
 && rm -rf /var/lib/apt/lists/*

RUN cd /root/gdb-$GDB_VERSION \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --prefix=/opt \
      --with-gmp=/opt \
      --with-mpfr=/opt \
      --with-mpc=/opt \
      --with-python=python3 \
      --with-tui \
 && make -j \
 && make install-strip \
 ;

RUN cd /root/gcc-$GCC_VERSION \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --prefix=/opt \
      --enable-checking=release \
      --enable-languages=c,c++ \
      --disable-multilib \
 && make -j \
 && make install-strip \
 ;

### toolchain-mingw ############################################################

FROM toolchain-common AS toolchain-mingw

RUN cd /root/binutils-$BINUTILS_VERSION \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --target=x86_64-w64-mingw32 \
      --prefix=/opt \
      --disable-multilib \
 && make -j \
 && make install-strip \
 && rm -rf /root/binutils-gdb \
 ;

ENV PATH="/opt/bin/:$PATH"

ARG MINGW_VERSION=v12.0.0
RUN cd /root \
 && wget https://sourceforge.net/projects/mingw-w64/files/mingw-w64/mingw-w64-release/mingw-w64-$MINGW_VERSION.tar.bz2/download \
      -O mingw-w64.tar.bz2 \
 && tar xvf mingw-w64.tar.bz2 \
 && rm mingw-w64.tar.bz2 \
 ;

RUN cd /root/mingw-w64-$MINGW_VERSION/mingw-w64-headers \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --host=x86_64-w64-mingw32 \
      --prefix=/opt/x86_64-w64-mingw32 \
 && make install \
 ;

RUN cd /root/gcc-$GCC_VERSION \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --target=x86_64-w64-mingw32 \
      --prefix=/opt \
      --enable-checking=release \
      --enable-languages=c,c++ \
      --disable-multilib \
 && make -j all-gcc \
 && make install-strip-gcc \
 ;

RUN cd /root/mingw-w64-$MINGW_VERSION \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --host=x86_64-w64-mingw32 \
      --prefix=/opt/x86_64-w64-mingw32 \
 && make -j \
 && make install-strip \
 ;

RUN cd /root/gcc-$GCC_VERSION/build \
 && make -j \
 && make install-strip \
 ;

RUN cd /root/mingw-w64-$MINGW_VERSION/mingw-w64-libraries/winpthreads \
 && mkdir build \
 && cd build \
 && ../configure -v \
     --host=x86_64-w64-mingw32 \
     --prefix=/opt/x86_64-w64-mingw32 \
     --enable-shared \
 && make -j \
 && make install-strip \
 ;

RUN ln -sf x86_64-w64-mingw32-gcc /opt/bin/gcc \
 && ln -sf x86_64-w64-mingw32-g++ /opt/bin/g++ \
 && ln -sf x86_64-w64-mingw32-ld /opt/bin/ld \
 && ln -sf x86_64-w64-mingw32-ar /opt/bin/ar \
 && ln -sf x86_64-w64-mingw32-nm /opt/bin/nm \
 && ln -sf x86_64-w64-mingw32-objcopy /opt/bin/objcopy \
 && ln -sf x86_64-w64-mingw32-objdump /opt/bin/objdump \
 && ln -sf x86_64-w64-mingw32-ranlib /opt/bin/ranlib \
 && ln -sf x86_64-w64-mingw32-strip /opt/bin/strip \
 ;

### linux ######################################################################

FROM common AS linux

COPY --from=toolchain-linux /opt/ /opt/toolchain/
ADD toolchain-env.sh /opt/toolchain/env.sh

ARG LIBFFI_VERSION=3.4.7
RUN cd /root \
 && wget https://github.com/libffi/libffi/releases/download/v$LIBFFI_VERSION/libffi-$LIBFFI_VERSION.tar.gz \
 && tar xvf libffi-$LIBFFI_VERSION.tar.gz \
 && cd libffi-$LIBFFI_VERSION \
 && mkdir build \
 && cd build \
 && . /opt/toolchain/env.sh \
 && ../configure -v \
      --prefix=/opt/toolchain \
      --disable-docs \
      CFLAGS="$CFLAGS -O3 -fPIC" \
      CXXFLAGS="$CXXFLAGS -DFFI_BUILDING_DLL -O3 -fPIC" \
 && make -j \
 && make install \
 && rm -rf /root/* \
 ;

ARG GTEST_VERSION=v1.16.0
RUN git clone --depth=1 --branch=$GTEST_VERSION https://github.com/google/googletest.git \
 && cd googletest \
 && . /opt/toolchain/env.sh \
 && cmake -B build -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/toolchain \
 && ninja -C build \
 && ninja -C build install \
 && rm -rf /root/* \
 ;

RUN apt-get update && apt-get install -y --no-install-recommends \
    libjson-c-dev \
 && rm -rf /var/lib/apt/lists/*

ADD install_spall.sh /root/
RUN /root/install_spall.sh /usr/local
RUN rm /root/install_spall.sh

### mingw ######################################################################

FROM common AS mingw

COPY --from=toolchain-mingw /opt/ /opt/toolchain/
ADD toolchain-env.sh /opt/toolchain/env.sh

RUN apt-get update && apt-get install -y --no-install-recommends \
    wine \
    winetricks \
 && rm -rf /var/lib/apt/lists/*

ENV WINEPATH=/opt/toolchain/x86_64-w64-mingw32/bin
ENV WINEDEBUG=-all

ARG LIBFFI_VERSION=3.4.7
RUN cd /root \
 && wget https://github.com/libffi/libffi/releases/download/v$LIBFFI_VERSION/libffi-$LIBFFI_VERSION.tar.gz \
 && tar xvf libffi-$LIBFFI_VERSION.tar.gz \
 && cd libffi-$LIBFFI_VERSION \
 && mkdir build \
 && cd build \
 && . /opt/toolchain/env.sh \
 && ../configure -v \
      --host=x86_64-w64-mingw32 \
      --prefix=/opt/toolchain \
      --disable-docs \
      CFLAGS="$CFLAGS -O3 -fPIC" \
      CXXFLAGS="$CXXFLAGS -DFFI_BUILDING_DLL -O3 -fPIC" \
 && make -j \
 && make install \
 && rm -rf /root/* \
 ;

ARG DLFCN_VERSION=v1.4.2
RUN git clone --depth=1 --branch=$DLFCN_VERSION https://github.com/dlfcn-win32/dlfcn-win32 \
 && cd dlfcn-win32 \
 && . /opt/toolchain/env.sh \
 && ./configure \
      --prefix=/opt/toolchain \
      --enable-shared \
 && make -j \
 && make install \
 && rm -rf /root/* \
 ;

ARG GTEST_VERSION=v1.16.0
RUN git clone --depth=1 --branch=$GTEST_VERSION https://github.com/google/googletest.git \
 && cd googletest \
 && . /opt/toolchain/env.sh \
 && cmake -B build -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/toolchain \
 && ninja -C build \
 && ninja -C build install \
 && rm -rf /root/* \
 ;

ADD install_spall.sh /root/
RUN /root/install_spall.sh /opt/toolchain
RUN rm /root/install_spall.sh
