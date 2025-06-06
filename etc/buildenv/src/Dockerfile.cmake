FROM clang AS cmake

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl-dev \
 && rm -rf /var/lib/apt/lists/*

RUN --mount=type=cache,dst=${CWGET_CACHE_DIR} cd /root \
 && cwget {{CMAKE_URL}} \
 && tar xzf cmake-{{CMAKE_VERSION}}.tar.gz \
 && cd cmake-{{CMAKE_VERSION}} \
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
