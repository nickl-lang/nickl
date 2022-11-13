RUN apk add --no-cache \
    debootstrap \
    g++ \
    gmock \
    gtest-dev \
    libffi-dev \
    ;

RUN git clone https://github.com/yse/easy_profiler.git \
 && mkdir easy_profiler/build \
 && cd easy_profiler/build \
 && cmake -GNinja .. \
 && ninja install \
 && cd ../.. \
 && rm -rf easy_profiler \
 ;
