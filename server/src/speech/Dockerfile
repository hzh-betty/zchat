# ==========================================
# 阶段 1：Build 阶段（包含编译器和 Conan 2）
# ==========================================
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    python3 \
    python3-pip \
    git \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install "conan>=2.0.0"

WORKDIR /app
RUN conan profile detect --force
COPY conanfile.py .

ARG BUILD_PRESET=conan2-release

RUN if [ "$BUILD_PRESET" = "conan2-debug" ] ; then \
        conan install . --output-folder=build/conan2-debug --build=missing -s build_type=Debug ; \
    else \
        conan install . --output-folder=build/conan2-release --build=missing -s build_type=Release ; \
    fi

COPY CMakeLists.txt CMakePresets.json ./
COPY server ./server

RUN cmake --preset ${BUILD_PRESET} \
    && cmake --build --preset ${BUILD_PRESET} --target zchat_speech_service -j$(nproc)

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

COPY --from=builder /app/build/${BUILD_PRESET}/bin/zchat_speech_service /app/bin/zchat_speech_service
COPY --from=builder /app/server/config/speech.json /app/server/config/speech.json

CMD ["/app/bin/zchat_speech_service"]
