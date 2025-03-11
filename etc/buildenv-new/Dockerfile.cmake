FROM base AS cmake

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl-dev \
 && rm -rf /var/lib/apt/lists/*

ARG CMAKE_VERSION=3.31.6
RUN --mount=type=cache,dst=$CWGET_CACHE_DIR cd /root \
 && cwget http://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION.tar.gz \
 && tar xzf cmake-$CMAKE_VERSION.tar.gz \
 && rm cmake-$CMAKE_VERSION.tar.gz \
 ;

RUN cd /root/cmake-$CMAKE_VERSION \
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

# vim: ft=dockerfile
