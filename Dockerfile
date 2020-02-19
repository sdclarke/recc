FROM registry.gitlab.com/buildgrid/buildbox/buildbox-common:latest

ARG ENABLE_TESTS=true
ARG EXTRA_CMAKE_FLAGS=
ENV TEST_DEPENDS=${TEST:+googletest}

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    google-mock \
    libssl-dev \
    make \
    pkg-config \
    "${TEST_DEPENDS}" \
    && apt-get clean


WORKDIR /recc
COPY . .

# "build" directory is in .dockerignore
RUN mkdir build && cd build && \
    if [ -z "${ENABLE_TESTS}" ]; then \
        echo "Building without tests" && \
        cmake -DBUILD_ENABLE_TESTSING=OFF "${EXTRA_CMAKE_FLAGS}" .. && \
        make -j$(nproc) && \
        find /recc/build/bin/; \
    else \
        echo "Building with tests" && \
        cmake -DGTEST_SOURCE_ROOT=/usr/src/googletest "${EXTRA_CMAKE_FLAGS}" .. && \
        make -j$(nproc) && ctest --verbose && \
        find /recc/build/bin/; \
    fi

# Extend PATH to include the recc binaries
ENV PATH "/recc/build/bin:$PATH"
