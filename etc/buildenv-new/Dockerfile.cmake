FROM base AS cmake

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl-dev \
 && rm -rf /var/lib/apt/lists/*

ARG CMAKE_VERSION=3.31.6
RUN cd /root \
 && wget http://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION.tar.gz \
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

# vim: ft=dockerfile
