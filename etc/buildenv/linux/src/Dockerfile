RUN apt-get update && apt-get install -y --no-install-recommends \
    libffi-dev \
    libgmock-dev \
    libgtest-dev \
 && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/yse/easy_profiler.git \
 && mkdir easy_profiler/build \
 && cd easy_profiler/build \
 && cmake -GNinja .. \
 && ninja install \
 && cd ../.. \
 && rm -rf easy_profiler \
 ;
