#!/bin/bash

# Build Crazy Framework Script (Linux)

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/out_linux"

echo "========================================"
echo "   Crazy Framework Build Script (Linux)"
echo "========================================"

echo "Project Root: $PROJECT_ROOT"
echo "Build Directory: $BUILD_DIR"

# 创建构建目录
mkdir -p "$BUILD_DIR"

# 检查 CMake
if ! command -v cmake &> /dev/null; then
    echo "Error: CMake is not installed or not in PATH"
    exit 1
fi

echo ""
echo "========================================"
echo "Building x86 Debug Version..."
echo "========================================"

cd "$BUILD_DIR"
mkdir -p x86_debug
cd x86_debug

cmake "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-m32" -DCMAKE_C_FLAGS="-m32"
if [ $? -eq 0 ]; then
    echo "CMake configuration successful for x86 Debug"
    make -j$(nproc)
    if [ $? -eq 0 ]; then
        echo "x86 Debug build successful!"
    else
        echo "x86 Debug build failed!"
    fi
else
    echo "CMake configuration failed for x86 Debug"
fi

echo ""
echo "========================================"
echo "Building x86 Release Version..."
echo "========================================"

cd "$BUILD_DIR"
mkdir -p x86_release
cd x86_release

cmake "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-m32 -O2" -DCMAKE_C_FLAGS="-m32 -O2"
if [ $? -eq 0 ]; then
    echo "CMake configuration successful for x86 Release"
    make -j$(nproc)
    if [ $? -eq 0 ]; then
        echo "x86 Release build successful!"
    else
        echo "x86 Release build failed!"
    fi
else
    echo "CMake configuration failed for x86 Release"
fi

echo ""
echo "========================================"
echo "Building x64 Debug Version..."
echo "========================================"

cd "$BUILD_DIR"
mkdir -p x64_debug
cd x64_debug

cmake "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Debug
if [ $? -eq 0 ]; then
    echo "CMake configuration successful for x64 Debug"
    make -j$(nproc)
    if [ $? -eq 0 ]; then
        echo "x64 Debug build successful!"
    else
        echo "x64 Debug build failed!"
    fi
else
    echo "CMake configuration failed for x64 Debug"
fi

echo ""
echo "========================================"
echo "Building x64 Release Version..."
echo "========================================"

cd "$BUILD_DIR"
mkdir -p x64_release
cd x64_release

cmake "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O2" -DCMAKE_C_FLAGS="-O2"
if [ $? -eq 0 ]; then
    echo "CMake configuration successful for x64 Release"
    make -j$(nproc)
    if [ $? -eq 0 ]; then
        echo "x64 Release build successful!"
    else
        echo "x64 Release build failed!"
    fi
else
    echo "CMake configuration failed for x64 Release"
fi

echo ""
echo "========================================"
echo "Build process completed!"
echo "========================================"
