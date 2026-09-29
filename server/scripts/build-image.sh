#!/bin/sh
set -eu

case "${BUILD_PRESET}" in
    conan2-debug) host_profile=profiles/linux-clang-debug ;;
    conan2-release) host_profile=profiles/linux-clang-release ;;
    *) echo "Unsupported BUILD_PRESET: ${BUILD_PRESET}" >&2; exit 1 ;;
esac

attempt=1
until conan build . --output-folder="/app/build/${BUILD_PRESET}" \
    --build=missing -pr:h "$host_profile" -pr:b profiles/linux-clang-release \
    -c:a "tools.build:jobs=${BUILD_JOBS}" \
    -cc core.graph:compatibility_mode=optimized \
    -cc core.download:retry=5 -cc core.download:retry_wait=3 \
    --deployer=conan/deployers/runtime.py --deployer-folder=/app/runtime
do
    if [ "$attempt" -ge 3 ]; then
        exit 1
    fi
    attempt=$((attempt + 1))
    echo "Conan build failed; retrying from cache (attempt ${attempt}/3)" >&2
    sleep 3
done

# Cache mounts are not part of the image: copy deliverables out before RUN exits.
mkdir -p /app/runtime/bin /app/runtime/lib/plugin
cp /app/build/"${BUILD_PRESET}"/bin/zchat_* /app/runtime/bin/
