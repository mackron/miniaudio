# Set MSVC_WINE_ROOT to the MSVC installation containing bin/x64.
set(MSVC_WINE_ROOT "$ENV{MSVC_WINE_ROOT}")
if(NOT MSVC_WINE_ROOT)
    message(FATAL_ERROR "Set MSVC_WINE_ROOT to the MSVC installation containing bin/x64.")
endif()

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_C_COMPILER   "${MSVC_WINE_ROOT}/bin/x64/cl")
set(CMAKE_CXX_COMPILER "${MSVC_WINE_ROOT}/bin/x64/cl")
set(CMAKE_AR           "${MSVC_WINE_ROOT}/bin/x64/lib")
set(CMAKE_LINKER       "${MSVC_WINE_ROOT}/bin/x64/link")
set(CMAKE_RC_COMPILER  "${MSVC_WINE_ROOT}/bin/x64/rc")
set(CMAKE_MT           "${MSVC_WINE_ROOT}/bin/x64/mt")

# Store compiler debug information in object files instead of a compiler PDB.
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")

# Host headers and libraries cannot be used by MSVC. Host build tools can.
set(CMAKE_FIND_ROOT_PATH "${MSVC_WINE_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Prevent pkg-config from importing Linux headers and libraries into Windows builds.
set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig TRUE)
