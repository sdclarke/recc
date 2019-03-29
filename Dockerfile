FROM grpc/cxx:1.12.0

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    google-mock \
    googletest \
    libcurl4-openssl-dev \
    libssl-dev \
    make \
    pkg-config \
    && apt-get clean


WORKDIR /recc
COPY . .

# Make sure we are not carrying over the local "build" directory
RUN rm -fr build || true && \
    mkdir build && cd build && \
    cmake .. -DGTEST_SOURCE_ROOT=/usr/src/googletest && \
    make -j$(nproc) && make test

# Extend PATH to include the recc binaries
ENV PATH "/recc/build/bin:$PATH"
