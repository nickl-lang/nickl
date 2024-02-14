LABEL org.opencontainers.image.description="Linux build environment for Nickl compiler"

RUN apt-get update && apt-get install -y --no-install-recommends \
    libffi-dev \
    libgmock-dev \
    libgtest-dev \
    libjson-c-dev \
 && rm -rf /var/lib/apt/lists/*

ADD common/scripts/install_spall.sh /root/
RUN /root/install_spall.sh /usr
RUN rm /root/install_spall.sh
