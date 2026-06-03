# ==========================================
# 阶段 1：Build 阶段（包含编译器和 Conan 2）
# ==========================================
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    clang \
    cmake \
    ninja-build \
    pkg-config \
    python3 \
    python3-pip \
    git \
    && rm -rf /var/lib/apt/lists/*

ENV CC=clang
ENV CXX=clang++

RUN pip3 install "conan>=2.28"

WORKDIR /app
RUN conan profile detect --force
COPY conanfile.py .

ARG BUILD_PRESET=conan2-release

RUN if [ "$BUILD_PRESET" = "conan2-debug" ] ; then \
        conan install . --output-folder=build/conan2-debug --build=missing -s:h build_type=Debug -s:b build_type=Release -s:a compiler.cppstd=17 -c:a tools.cmake.cmaketoolchain:generator=Ninja -c:a 'tools.build:compiler_executables={"c": "clang", "cpp": "clang++"}' -c tools.build:jobs=4 ; \
    else \
        conan install . --output-folder=build/conan2-release --build=missing -s:h build_type=Release -s:b build_type=Release -s:a compiler.cppstd=17 -c:a tools.cmake.cmaketoolchain:generator=Ninja -c:a 'tools.build:compiler_executables={"c": "clang", "cpp": "clang++"}' -c tools.build:jobs=4 ; \
    fi

COPY CMakeLists.txt CMakePresets.json ./
COPY server ./server

RUN cmake --preset ${BUILD_PRESET} \
    && cmake --build --preset ${BUILD_PRESET} --target zchat_transmite_service -j1

# ==========================================
# 阶段 2：Runner 运行阶段（极致轻量运行环境）
# ==========================================
FROM ubuntu:22.04 AS runner

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    ca-certificates \
    libuuid1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

ARG BUILD_PRESET=conan2-release

COPY --from=builder /app/build/${BUILD_PRESET}/bin/zchat_transmite_service /app/bin/zchat_transmite_service
COPY --from=builder /app/server/config/transmite.json /app/server/config/transmite.json

CMD ["/app/bin/zchat_transmite_service"]
