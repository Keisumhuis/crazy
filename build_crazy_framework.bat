@echo off
chcp 65001 >nul
title Build Crazy Framework (VS2022)

set PROJECT_ROOT=%cd%
set BUILD_DIR=%PROJECT_ROOT%/out_windows

echo ========================================
echo    Crazy Framework Build Script (VS2022)
echo ========================================

:: 创建构建目录
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo.
echo ========================================
echo 1. Building x86 Debug (VS2022)...
echo ========================================

cd /d "%BUILD_DIR%"
if not exist "x86_debug" mkdir "x86_debug"
cd /d "%BUILD_DIR%/x86_debug"

cmake "%PROJECT_ROOT%" -G "Visual Studio 17 2022" -A Win32
if %errorlevel% equ 0 (
    cmake --build . --config Debug
    if %errorlevel% equ 0 (
        echo x86 Debug build successful!
    ) else (
        echo x86 Debug build failed!
    )
)

echo.
echo ========================================
echo 2. Building x86 Release (VS2022)...
echo ========================================

cd /d "%BUILD_DIR%"
if not exist "x86_release" mkdir "x86_release"
cd /d "%BUILD_DIR%/x86_release"

cmake "%PROJECT_ROOT%" -G "Visual Studio 17 2022" -A Win32
if %errorlevel% equ 0 (
    cmake --build . --config Release
    if %errorlevel% equ 0 (
        echo x86 Release build successful!
    ) else (
        echo x86 Release build failed!
    )
)

echo.
echo ========================================
echo 3. Building x64 Debug (VS2022)...
echo ========================================

cd /d "%BUILD_DIR%"
if not exist "x64_debug" mkdir "x64_debug"
cd /d "%BUILD_DIR%/x64_debug"

cmake "%PROJECT_ROOT%" -G "Visual Studio 17 2022" -A x64
if %errorlevel% equ 0 (
    cmake --build . --config Debug
    if %errorlevel% equ 0 (
        echo x64 Debug build successful!
    ) else (
        echo x64 Debug build failed!
    )
)

echo.
echo ========================================
echo 4. Building x64 Release (VS2022)...
echo ========================================

cd /d "%BUILD_DIR%"
if not exist "x64_release" mkdir "x64_release"
cd /d "%BUILD_DIR%/x64_release"

cmake "%PROJECT_ROOT%" -G "Visual Studio 17 2022" -A x64
if %errorlevel% equ 0 (
    cmake --build . --config Release
    if %errorlevel% equ 0 (
        echo x64 Release build successful!
    ) else (
        echo x64 Release build failed!
    )
)

echo.
echo ========================================
echo All builds completed!
echo ========================================

pause