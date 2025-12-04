#!/usr/bin/env bash

set -e

ABIS=(arm64-v8a armeabi-v7a x86 x86_64)
BUILD_TYPE=${BUILD_TYPE:-Release}
ANDROID_PLATFORM=${ANDROID_PLATFORM:-android-21}

check_ndk() {
    echo " - Checking Android NDK..."
    [[ -z "$ANDROID_NDK" ]] && echo "[!] ANDROID_NDK not set" && exit 1
    echo "[*] ANDROID_NDK is set to: $ANDROID_NDK"
    [[ ! -f "$ANDROID_NDK/build/cmake/android.toolchain.cmake" ]] && echo "[!] Invalid NDK path - toolchain not found" && exit 1
    echo "[*] NDK toolchain found"
}

build_abi() {
    local ABI=$1
    local BUILD_DIR="build/$ABI"

    echo "[*] Building for $ABI..."

    # Clean existing build directory
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    echo " - Working in: $(pwd)"

    echo "[*] Configuring CMake..."
    cmake \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        ../..

    echo "[*] Building..."
    cmake --build . -j"$(nproc 2>/dev/null || echo 4)"

    # Create bin directory structure instead of libs
    mkdir -p "../../bin/$ABI"

    # Check if the executable exists and copy it
    if [[ -f "bin/task_optimizer" ]]; then
        cp bin/task_optimizer "../../bin/$ABI/"
        echo "[*] Built and copied task_optimizer for $ABI"
    else
        echo "[!] Error: task_optimizer not found in build/$ABI/bin/"
        echo "Contents of build/$ABI/:"
        ls -la
        echo "Contents of build/$ABI/bin/ (if exists):"
        ls -la bin/ 2>/dev/null || echo "bin/ directory doesn't exist"
        exit 1
    fi

    cd ../..
}

case "${1:-all}" in
    "all")
        check_ndk
        # Clean all builds if requested or do selective cleaning
        if [[ "$2" == "clean" ]]; then
            rm -rf build/ bin/
        else
            # Clean only the architectures we're about to build
            for ABI in "${ABIS[@]}"; do
                rm -rf "build/$ABI" "bin/$ABI"
            done
        fi
        for ABI in "${ABIS[@]}"; do
            build_abi "$ABI"
        done
        ;;
    "clean")
        rm -rf build/ bin/
        ;;
    *)
        check_ndk
        [[ " ${ABIS[*]} " == *" $1 "* ]] && build_abi "$1" || echo "Invalid ABI: $1"
        ;;
esac
