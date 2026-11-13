# Set COSMOCC_ROOT to the extracted cosmocc directory and add its bin directory to PATH.
set(COSMOCC_ROOT "$ENV{COSMOCC_ROOT}")
if(NOT COSMOCC_ROOT)
    message(FATAL_ERROR "Set COSMOCC_ROOT to the extracted cosmocc directory.")
endif()

# Use Linux build rules for cosmo's static POSIX libraries and executables.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER   "${COSMOCC_ROOT}/bin/x86_64-unknown-cosmo-cc")
set(CMAKE_CXX_COMPILER "${COSMOCC_ROOT}/bin/x86_64-unknown-cosmo-c++")
set(CMAKE_AR           "${COSMOCC_ROOT}/bin/x86_64-linux-cosmo-ar")
set(CMAKE_RANLIB       "${COSMOCC_ROOT}/bin/x86_64-linux-cosmo-ranlib")

set(CMAKE_C_FLAGS_INIT   "-mcosmo")
set(CMAKE_CXX_FLAGS_INIT "-mcosmo")
set(CMAKE_SKIP_RPATH ON)

# Cosmo headers require C99, and stack protection uses incompatible thread-local storage. Put these
# flags after target options so dependencies cannot override them.
set(CMAKE_C_COMPILE_OBJECT "<CMAKE_C_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -std=c99 -fno-stack-protector -flarge-source-files -o <OBJECT> -c <SOURCE>")
set(CMAKE_CXX_COMPILE_OBJECT "<CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -fno-stack-protector -flarge-source-files -o <OBJECT> -c <SOURCE>")

# Host headers and libraries cannot be used by cosmo. Host build tools can.
set(CMAKE_FIND_ROOT_PATH "${COSMOCC_ROOT}" "${COSMOCC_ROOT}/x86_64-linux-cosmo")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Prevent pkg-config from importing Linux headers and libraries into cosmo builds.
set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig TRUE)

