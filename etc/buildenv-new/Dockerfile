### base #######################################################################

FROM ubuntu:20.04 AS base

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    ccache \
    curl \
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
      --disable-gprofng \
      --program-prefix=x86_64-pc-linux-gnu- \
 && make -j \
 && make install-strip \
 && make clean \
 ;

ENV PATH="/opt/bin:$PATH"

RUN apt-get update && apt-get install -y --no-install-recommends \
    m4 \
 && rm -rf /var/lib/apt/lists/*

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
 && rm -rf ./* \
 ;

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3-dev \
 && rm -rf /var/lib/apt/lists/*

# TODO: Move gdb download to start
ARG GDB_VERSION=16.2
RUN cd /root/ \
 && wget https://ftp.gnu.org/gnu/gdb/gdb-$GDB_VERSION.tar.xz \
 && tar xvf gdb-$GDB_VERSION.tar.xz \
 && rm gdb-$GDB_VERSION.tar.xz \
 ;

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
 && rm -rf /root/gdb-$GDB_VERSION \
 ;

### toolchain-mingw-common #####################################################

FROM toolchain-common AS toolchain-mingw-common

ARG MINGW_VERSION=v12.0.0
RUN cd /root \
 && wget https://sourceforge.net/projects/mingw-w64/files/mingw-w64/mingw-w64-release/mingw-w64-$MINGW_VERSION.tar.bz2/download \
      -O mingw-w64.tar.bz2 \
 && tar xvf mingw-w64.tar.bz2 \
 && rm mingw-w64.tar.bz2 \
 ;

### toolchain-mingw ############################################################

FROM toolchain-mingw-common AS toolchain-mingw

RUN cd /root/binutils-$BINUTILS_VERSION \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --target=x86_64-w64-mingw32 \
      --prefix=/opt \
      --disable-multilib \
      --disable-gprofng \
 && make -j \
 && make install-strip \
 && make clean \
 ;

ENV PATH="/opt/bin:$PATH"

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
 && make clean \
 ;

RUN cd /root/gcc-$GCC_VERSION/build \
 && make -j \
 && make install-strip \
 && rm -rf ./* \
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
 && make clean \
 ;

### linux ######################################################################

FROM common AS linux

COPY --from=toolchain-linux /opt/ /opt/toolchain/
ADD toolchain-env-linux.sh /opt/toolchain/env.sh

ENV CMAKE_TOOLCHAIN_FILE=/opt/toolchain/toolchain.cmake
ADD toolchain-linux.cmake $CMAKE_TOOLCHAIN_FILE

ENV PATH="/opt/toolchain/bin:$PATH"
ENV LD_LIBRARY_PATH="/opt/toolchain/lib:/opt/toolchain/lib64:$LD_LIBRARY_PATH"

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
RUN cd /root \
 && git clone --depth=1 --branch=$GTEST_VERSION https://github.com/google/googletest.git \
 && cd googletest \
 && cmake -B build -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/toolchain \
 && ninja -C build \
 && ninja -C build install \
 && rm -rf /root/* \
 ;

ARG JSON_C_VERSION=json-c-0.18-20240915
RUN cd /root \
 && git clone --depth=1 --branch=$JSON_C_VERSION https://github.com/json-c/json-c \
 && cd json-c \
 && cmake -B build -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/toolchain \
 && ninja -C build \
 && ninja -C build install \
 && rm -rf /root/* \
 ;

ADD install_spall.sh /root/
RUN /root/install_spall.sh /usr/local
RUN rm /root/install_spall.sh

### toolchain-mingw-native #####################################################

FROM toolchain-mingw-common AS toolchain-mingw-native

COPY --from=toolchain-mingw /opt/ /opt/toolchain/

ENV PATH="/opt/toolchain/bin:$PATH"

RUN cd /root/binutils-$BINUTILS_VERSION \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --host=x86_64-w64-mingw32 \
      --target=x86_64-w64-mingw32 \
      --prefix=/opt/toolchain-native \
      --disable-multilib \
      --disable-gprofng \
 && make -j \
 && make install-strip \
 && make clean \
 ;

RUN cd /root/gcc-$GCC_VERSION \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --host=x86_64-w64-mingw32 \
      --target=x86_64-w64-mingw32 \
      --prefix=/opt/toolchain-native \
      --enable-checking=release \
      --enable-languages=c,c++ \
      --disable-multilib \
 && make -j \
 && make install-strip \
 && rm -rf ./* \
 ;

RUN mv /opt/toolchain-native/lib/libgcc_s_seh-1.dll /opt/toolchain-native/bin/

RUN cd /root/mingw-w64-$MINGW_VERSION \
 && mkdir build \
 && cd build \
 && ../configure -v \
      --host=x86_64-w64-mingw32 \
      --prefix=/opt/toolchain-native/x86_64-w64-mingw32 \
      --with-libraries=winpthreads \
 && make -j \
 && make install-strip \
 && make clean \
 ;

RUN mv /opt/toolchain-native/x86_64-w64-mingw32/bin/libwinpthread-1.dll /opt/toolchain-native/bin/

### mingw ######################################################################

FROM common AS mingw

COPY --from=toolchain-mingw /opt/ /opt/toolchain/
ADD toolchain-env-mingw.sh /opt/toolchain/env.sh

ENV CMAKE_TOOLCHAIN_FILE=/opt/toolchain/toolchain.cmake
ADD toolchain-mingw.cmake $CMAKE_TOOLCHAIN_FILE

ENV PATH="/opt/toolchain/bin:$PATH"
ENV LD_LIBRARY_PATH="/opt/toolchain/lib:/opt/toolchain/lib64:$LD_LIBRARY_PATH"

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
RUN cd /root \
 && git clone --depth=1 --branch=$DLFCN_VERSION https://github.com/dlfcn-win32/dlfcn-win32 \
 && cd dlfcn-win32 \
 && . /opt/toolchain/env.sh \
 && ./configure \
      --prefix=/opt/toolchain \
      --enable-shared \
      --cross-prefix=x86_64-w64-mingw32- \
 && make -j \
 && make install \
 && rm -rf /root/* \
 ;

ARG GTEST_VERSION=v1.16.0
RUN cd /root \
 && git clone --depth=1 --branch=$GTEST_VERSION https://github.com/google/googletest.git \
 && cd googletest \
 && cmake -B build -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/toolchain \
 && ninja -C build \
 && ninja -C build install \
 && rm -rf /root/* \
 ;

COPY --from=toolchain-mingw-native /opt/toolchain-native /opt/toolchain-native

ARG JSON_C_VERSION=json-c-0.18-20240915
RUN cd /root \
 && git clone --depth=1 --branch=$JSON_C_VERSION https://github.com/json-c/json-c \
 && cd json-c \
 && cmake -B build -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/toolchain/x86_64-w64-mingw32 \
 && ninja -C build \
 && ninja -C build install \
 && rm -rf /root/* \
 ;

RUN apt-get update && apt-get install -y --no-install-recommends \
    wine \
    winetricks \
 && rm -rf /var/lib/apt/lists/*

ENV WINEPATH="/opt/toolchain/x86_64-w64-mingw32/bin;/opt/toolchain-native/bin"
ENV WINEDEBUG=-all

ADD install_spall.sh /root/
RUN /root/install_spall.sh /opt/toolchain
RUN rm /root/install_spall.sh
