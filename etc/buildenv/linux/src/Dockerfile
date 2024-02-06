RUN apt-get update && apt-get install -y --no-install-recommends \
    libffi-dev \
    libgmock-dev \
    libgtest-dev \
    libjson-c-dev \
 && rm -rf /var/lib/apt/lists/*

RUN wget -O /usr/include/spall.h \
    "https://raw.githubusercontent.com/colrdavidson/spall-web/5129ce6778a9d732a7de727a79648a71d1092962/spall.h"
