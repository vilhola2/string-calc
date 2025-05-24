#!/bin/sh

BUILD_TYPE=RELEASE
BUILD_EXECUTABLE=ON

for arg in "$@"; do
  case $arg in
    -d)
      BUILD_TYPE=DEBUG
      ;;
    -l)
      BUILD_EXECUTABLE=OFF
      ;;
  esac
done

if command -v clang >/dev/null && clang --version | grep -E "version (1[89]|[2-9][0-9])" >/dev/null; then
    export CC=clang
elif command -v gcc >/dev/null && gcc -dumpversion | grep -E "^(1[4-9]|[2-9][0-9])" >/dev/null; then
    export CC=gcc
else
    echo "No suitable C compiler found. Please install Clang >= 18 or GCC >= 14." >&2
    exit 1
fi

echo "Using compiler: $CC"
echo "Using build type: $BUILD_TYPE"

mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_EXECUTABLE=$BUILD_EXECUTABLE ..
make
