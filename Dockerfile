FROM grpc/cxx:1.12.0

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    google-mock \
    googletest \
    libssl-dev \
    make \
    pkg-config \
    && apt-get clean


WORKDIR /recc
COPY . .
RUN mkdir build && cd build && cmake .. -DGTEST_SOURCE_ROOT=/usr/src/googletest && make -j$(nproc) && make test
