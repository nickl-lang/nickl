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

### gcc ########################################################################

FROM common AS gcc

ARG BINUTILS_VERSION=2.44
ARG GCC_VERSION=14.2.0

RUN apt-get update && apt-get install -y --no-install-recommends \
    zstd \
 && rm -rf /var/lib/apt/lists/*

RUN cd /root \
 && wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.zst \
 && tar xvf binutils-$BINUTILS_VERSION.tar.zst \
 && rm binutils-$BINUTILS_VERSION.tar.zst \
 ;

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
 ;

RUN cd /root/gcc-$GCC_VERSION \
 && ./contrib/download_prerequisites \
 ;

RUN cd /root/gcc-$GCC_VERSION \
 && ./configure -v \
     --prefix=/opt \
     --enable-checking=release \
     --enable-languages=c,c++ \
     --disable-multilib \
 && make -j \
 && make install-strip \
 ;

### linux_base #################################################################

FROM common AS linux_base

COPY --from=gcc /opt/ /opt/gcc/
ADD toolchain_gcc.sh /opt/gcc/toolchain.sh

### libffi #####################################################################

FROM linux_base AS libffi

ARG M4_VERSION=1.4.19
RUN cd /root \
 && wget https://ftp.gnu.org/gnu/m4/m4-$M4_VERSION.tar.gz \
 && tar xvf m4-$M4_VERSION.tar.gz \
 && cd m4-$M4_VERSION \
 && . /opt/gcc/toolchain.sh \
 && ./configure --prefix=/usr/local \
 && make -j \
 && make install \
 && rm -rf /root/* \
 ;

RUN apt-get update && apt-get install -y --no-install-recommends \
    automake \
    libltdl-dev \
    libtool \
 && rm -rf /var/lib/apt/lists/*

ARG AUTOCONF_VERSION=2.72
RUN cd /root \
 && wget http://ftp.gnu.org/gnu/autoconf/autoconf-$AUTOCONF_VERSION.tar.gz \
 && tar xvf autoconf-$AUTOCONF_VERSION.tar.gz \
 && cd autoconf-$AUTOCONF_VERSION \
 && . /opt/gcc/toolchain.sh \
 && ./configure --prefix=/usr/local \
 && make -j \
 && make install \
 && rm -rf /root/* \
 ;

ARG LIBFFI_VERSION=v3.4.7
RUN cd /root \
 && git clone --depth=1 --branch=$LIBFFI_VERSION https://github.com/libffi/libffi \
 && cd libffi \
 && . /opt/gcc/toolchain.sh \
 && ./autogen.sh \
 && ./configure \
     --prefix=/opt/libffi \
     --disable-docs \
     CFLAGS="$CFLAGS -fPIC" \
     CXXFLAGS="$CXXFLAGS -DFFI_BUILDING_DLL -fPIC" \
 && make -j \
 && make install \
 && rm -rf /root/* \
 ;

### linux ######################################################################

FROM linux_base AS linux

COPY --from=libffi /opt/libffi/ /usr/local/

RUN apt-get update && apt-get install -y --no-install-recommends \
    libgmock-dev \
    libgtest-dev \
    libjson-c-dev \
 && rm -rf /var/lib/apt/lists/*

ADD install_spall.sh /root/
RUN /root/install_spall.sh /usr
RUN rm /root/install_spall.sh
