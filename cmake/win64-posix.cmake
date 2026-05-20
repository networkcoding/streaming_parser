set(TOOLCHAIN_DIR "/usr")
set(TARGET_TRIPLE "x86_64-w64-mingw32")

if(NOT EXISTS "${TOOLCHAIN_DIR}")
  message(FATAL_ERROR "Toolchain directory does not exist: ${TOOLCHAIN_DIR}")
endif()

# Cross-compile target platform
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_SYSTEM_VERSION 1)

# Avoid try-run checks when cross compiling.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# BoringSSL and other code include <ws2tcpip.h>; inet_pton and others are only
# declared when _WIN32_WINNT is Vista or later. MinGW defaults to an older
# value, so set this explicitly for the whole cross build.
string(APPEND CMAKE_C_FLAGS_INIT " -D_WIN32_WINNT=0x0600")
string(APPEND CMAKE_CXX_FLAGS_INIT " -D_WIN32_WINNT=0x0600")

set(CMAKE_C_COMPILER "${TOOLCHAIN_DIR}/bin/${TARGET_TRIPLE}-gcc-posix")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_DIR}/bin/${TARGET_TRIPLE}-g++-posix")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_DIR}/bin/${TARGET_TRIPLE}-gcc-posix")
set(CMAKE_RANLIB "${TOOLCHAIN_DIR}/bin/${TARGET_TRIPLE}-gcc-ranlib-posix")
set(CMAKE_AR "${TOOLCHAIN_DIR}/bin/${TARGET_TRIPLE}-gcc-ar-posix")
set(CMAKE_NM "${TOOLCHAIN_DIR}/bin/${TARGET_TRIPLE}-gcc-nm-posix")

set(CMAKE_OBJCOPY "${TOOLCHAIN_DIR}/bin/${TARGET_TRIPLE}-objcopy")
set(CMAKE_OBJDUMP "${TOOLCHAIN_DIR}/bin/${TARGET_TRIPLE}-objdump")
set(CMAKE_DLLTOOL "${TOOLCHAIN_DIR}/bin/${TARGET_TRIPLE}-dlltool")
set(CMAKE_RC_COMPILER "${TOOLCHAIN_DIR}/bin/${TARGET_TRIPLE}-windres")

# MinGW sysroot and target search paths.
# /usr/lib/gcc/x86_64-w64-mingw32/10-posix
set(CMAKE_FIND_ROOT_PATH "${TOOLCHAIN_DIR}/${TARGET_TRIPLE}"
                         "${TOOLCHAIN_DIR}/lib/gcc/${TARGET_TRIPLE}/10-posix")

message(STATUS "WINDOWS64 ROOT PATH: ${CMAKE_FIND_ROOT_PATH}")

# Host tools from build machine; headers/libs from target sysroot.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
