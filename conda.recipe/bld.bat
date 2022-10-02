mkdir build
if %errorlevel% NEQ 0 exit /b %errorlevel%
cd build

ROBOCOPY %RECIPE_DIR%\libsolv %VCPKG_ROOT%\ports\libsolv
@rem ROBOCOPY has 0 and 1 as successfull exit codes
if %errorlevel% NEQ 0 if %errorlevel% NEQ 1 exit /b %errorlevel%

@rem Looks like the .vcpkg-root file is missing in vcpkg package
TYPE NUL > %VCPKG_ROOT%\.vcpkg-root

SET VCPKG_BUILD_TYPE=release
vcpkg install libsolv[conda] --triplet x64-windows-static
if %errorlevel% NEQ 0 exit /b %errorlevel%
vcpkg install "libarchive[bzip2,lz4,lzma,lzo,openssl,zstd]" --triplet x64-windows-static
if %errorlevel% NEQ 0 exit /b %errorlevel%
vcpkg install curl --triplet x64-windows-static
if %errorlevel% NEQ 0 exit /b %errorlevel%
vcpkg install yaml-cpp --triplet x64-windows-static
if %errorlevel% NEQ 0 exit /b %errorlevel%
vcpkg install reproc --triplet x64-windows-static
if %errorlevel% NEQ 0 exit /b %errorlevel%

SET "CXXFLAGS=%CXXFLAGS% /showIncludes"
SET CMAKE_PREFIX_PATH=%VCPKG_ROOT%\installed\x64-windows-static\;%CMAKE_PREFIX_PATH%

cmake .. ^
    -D CMAKE_INSTALL_PREFIX=%LIBRARY_PREFIX% ^
    -D CMAKE_PREFIX_PATH="%VCPKG_ROOT%\installed\x64-windows-static\;%CMAKE_PREFIX_PATH%" ^
    -D CMAKE_BUILD_TYPE="Release" ^
    -D BUILD_LIBMAMBA=ON ^
    -D BUILD_STATIC_DEPS=ON ^
    -D BUILD_MICROMAMBA=ON ^
    -D MICROMAMBA_LINKAGE=FULL_STATIC ^
    -G "Ninja"
if %errorlevel% NEQ 0 exit /b %errorlevel%

ninja install --verbose
if %errorlevel% NEQ 0 exit /b %errorlevel%
