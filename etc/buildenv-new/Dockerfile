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

### m4 #########################################################################

FROM common AS m4

ARG M4_VERSION=1.4.19
RUN cd /root \
 && wget https://ftp.gnu.org/gnu/m4/m4-$M4_VERSION.tar.gz \
 && tar xvf m4-$M4_VERSION.tar.gz \
 && cd m4-$M4_VERSION \
 && ./configure -v \
      --prefix=/opt \
 && make -j \
 && make install-strip \
 && rm -rf /root/* \
 ;
# TODO: Remove m4 dependency?

### gcc ########################################################################

FROM common AS gcc

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

RUN cd /root/binutils-$BINUTILS_VERSION \
 && ./configure -v \
     --prefix=/opt \
     --disable-multilib \
 && make -j \
 && make install-strip \
 && rm -rf /root/binutils-gdb \
 ;

RUN cd /root/gcc-$GCC_VERSION \
 && ./contrib/download_prerequisites \
 ;

COPY --from=m4 /opt/ /usr/local/

RUN cd /root/gcc-$GCC_VERSION/gmp \
 && ./configure -v \
     --prefix=/opt \
     --enable-shared \
 && make -j \
 && make install-strip \
 && make distclean \
 ;

RUN cd /root/gcc-$GCC_VERSION/mpfr \
 && ./configure -v \
     --prefix=/opt \
     --with-gmp=/opt \
     --enable-shared \
 && make -j \
 && make install-strip \
 && make distclean \
 ;

RUN cd /root/gcc-$GCC_VERSION/mpc \
 && ./configure -v \
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
 && ./configure -v \
     --prefix=/opt \
     --with-gmp=/opt \
     --with-mpfr=/opt \
     --with-mpc=/opt \
     --with-python=python3 \
     --with-tui \
 && make -j \
 && make install-strip \
 ;
 # TODO:  && make distclean \

RUN cd /root/gcc-$GCC_VERSION \
 && ./configure -v \
     --prefix=/opt \
     --enable-checking=release \
     --enable-languages=c,c++ \
     --disable-multilib \
 ;
 # TODO: merge:

RUN cd /root/gcc-$GCC_VERSION \
 && make -j \
 && make install-strip \
 ;
 # TODO:  && make distclean \

### linux_base #################################################################

FROM common AS linux_base

COPY --from=gcc /opt/ /opt/toolchain/
ADD toolchain_gcc.sh /opt/toolchain/env.sh

### linux ######################################################################

FROM linux_base AS linux

ARG LIBFFI_VERSION=3.4.7
RUN cd /root \
 && wget https://github.com/libffi/libffi/releases/download/v$LIBFFI_VERSION/libffi-$LIBFFI_VERSION.tar.gz \
 && tar xvf libffi-$LIBFFI_VERSION.tar.gz \
 && cd libffi-$LIBFFI_VERSION \
 && . /opt/toolchain/env.sh \
 && ./configure \
     --prefix=/usr/local \
     --disable-docs \
     CFLAGS="$CFLAGS -O3 -fPIC" \
     CXXFLAGS="$CXXFLAGS -DFFI_BUILDING_DLL -O3 -fPIC" \
 && make -j \
 && make install \
 && rm -rf /root/* \
 ;

RUN apt-get update && apt-get install -y --no-install-recommends \
    libgmock-dev \
    libgtest-dev \
    libjson-c-dev \
 && rm -rf /var/lib/apt/lists/*

ADD install_spall.sh /root/
RUN /root/install_spall.sh /usr
RUN rm /root/install_spall.sh
