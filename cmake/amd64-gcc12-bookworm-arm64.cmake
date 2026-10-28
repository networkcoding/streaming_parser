set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
# Install Debian 12 bookworm sysroot with the following command:
# ######################################################################################################################
# sudo debootstrap --variant=minbase \
# --include=libatomic1,curl,cmake,build-essential,git,gcc-aarch64-linux-gnu,g++-aarch64-linux-gnu --arch=amd64 bookworm
# /opt/sysroots/bookworm https://mirrors.tuna.tsinghua.edu.cn/ubuntu

set(CMAKE_SYSROOT "/opt/sysroots/bookworm")
set(CMAKE_LIBRARY_ARCHITECTURE "aarch64-linux-gnu")
set(_BOOKWORM_CROSS_ROOT "${CMAKE_SYSROOT}/usr/aarch64-linux-gnu")
set(_BOOKWORM_CROSS_GCC_ROOT "${CMAKE_SYSROOT}/usr/lib/gcc-cross/aarch64-linux-gnu/12")
set(_BOOKWORM_CROSS_BIN "${CMAKE_SYSROOT}/usr/bin")
set(_BOOKWORM_HOST_LIB_DIR "${CMAKE_SYSROOT}/usr/lib/x86_64-linux-gnu")
set(_BOOKWORM_TOOLCHAIN_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/bookworm-arm64-toolchain")
set(_BOOKWORM_BINUTILS_LIB_DIR "${_BOOKWORM_TOOLCHAIN_DIR}/host-libs")

foreach(
  _required_path
  "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-gcc-12"
  "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-g++-12"
  "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-ar"
  "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-ranlib"
  "${_BOOKWORM_HOST_LIB_DIR}/libbfd-2.40-arm64.so"
  "${_BOOKWORM_HOST_LIB_DIR}/libopcodes-2.40-arm64.so"
  "${_BOOKWORM_HOST_LIB_DIR}/libsframe.so.0"
  "${_BOOKWORM_CROSS_ROOT}/include/features.h"
  "${_BOOKWORM_CROSS_ROOT}/lib/libc.so"
  "${_BOOKWORM_CROSS_ROOT}/lib/ld-linux-aarch64.so.1"
  "${_BOOKWORM_CROSS_GCC_ROOT}/libstdc++.so"
)
  if(NOT EXISTS "${_required_path}")
    message(FATAL_ERROR "Debian 12 AArch64 toolchain file is missing required path: ${_required_path}")
  endif()
endforeach()

file(MAKE_DIRECTORY "${_BOOKWORM_BINUTILS_LIB_DIR}")
foreach(_binutils_library IN ITEMS libbfd-2.40-arm64.so libopcodes-2.40-arm64.so libsframe.so.0)
  if(NOT EXISTS "${_BOOKWORM_BINUTILS_LIB_DIR}/${_binutils_library}")
    file(
      CREATE_LINK
      "${_BOOKWORM_HOST_LIB_DIR}/${_binutils_library}"
      "${_BOOKWORM_BINUTILS_LIB_DIR}/${_binutils_library}"
      SYMBOLIC
    )
  endif()
endforeach()

function(_bookworm_make_tool_wrapper output_var wrapper_name real_tool)
  set(_wrapper "${_BOOKWORM_TOOLCHAIN_DIR}/${wrapper_name}")
  set(_wrapper_content "#!/bin/sh\n")
  string(APPEND _wrapper_content "if [ -n \"$LD_LIBRARY_PATH\" ]; then\n")
  string(APPEND _wrapper_content "  LD_LIBRARY_PATH=\"${_BOOKWORM_BINUTILS_LIB_DIR}:$LD_LIBRARY_PATH\"\n")
  string(APPEND _wrapper_content "else\n")
  string(APPEND _wrapper_content "  LD_LIBRARY_PATH=\"${_BOOKWORM_BINUTILS_LIB_DIR}\"\n")
  string(APPEND _wrapper_content "fi\nexport LD_LIBRARY_PATH\n")
  string(APPEND _wrapper_content "exec \"${real_tool}\" \"$@\"\n")
  file(WRITE "${_wrapper}" "${_wrapper_content}")
  file(
    CHMOD "${_wrapper}"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  )
  set(${output_var}
      "${_wrapper}"
      PARENT_SCOPE
  )
endfunction()

# wraps cross tools so they can load the Bookworm binutils host libraries without overriding the host libc
_bookworm_make_tool_wrapper(
  CMAKE_C_COMPILER
  gcc
  "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-gcc-12"
)
_bookworm_make_tool_wrapper(
  CMAKE_CXX_COMPILER
  g++
  "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-g++-12"
)
set(CMAKE_ASM_COMPILER "${CMAKE_C_COMPILER}")
_bookworm_make_tool_wrapper(CMAKE_AR ar "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-ar")
_bookworm_make_tool_wrapper(CMAKE_RANLIB ranlib "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-ranlib")
_bookworm_make_tool_wrapper(CMAKE_NM nm "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-nm")
_bookworm_make_tool_wrapper(CMAKE_LINKER ld "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-ld")
_bookworm_make_tool_wrapper(CMAKE_OBJCOPY objcopy "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-objcopy")
_bookworm_make_tool_wrapper(CMAKE_OBJDUMP objdump "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-objdump")
_bookworm_make_tool_wrapper(CMAKE_SIZE size "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-size")
_bookworm_make_tool_wrapper(CMAKE_STRIP strip "${_BOOKWORM_CROSS_BIN}/aarch64-linux-gnu-strip")

set(CMAKE_AR "${CMAKE_AR}" CACHE FILEPATH "Debian 12 AArch64 archiver" FORCE)
set(CMAKE_RANLIB "${CMAKE_RANLIB}" CACHE FILEPATH "Debian 12 AArch64 ranlib" FORCE)
set(CMAKE_NM "${CMAKE_NM}" CACHE FILEPATH "Debian 12 AArch64 nm" FORCE)
set(CMAKE_LINKER "${CMAKE_LINKER}" CACHE FILEPATH "Debian 12 AArch64 linker" FORCE)
set(CMAKE_OBJCOPY "${CMAKE_OBJCOPY}" CACHE FILEPATH "Debian 12 AArch64 objcopy" FORCE)
set(CMAKE_OBJDUMP "${CMAKE_OBJDUMP}" CACHE FILEPATH "Debian 12 AArch64 objdump" FORCE)
set(CMAKE_SIZE "${CMAKE_SIZE}" CACHE FILEPATH "Debian 12 AArch64 size" FORCE)
set(CMAKE_STRIP "${CMAKE_STRIP}" CACHE FILEPATH "Debian 12 AArch64 strip" FORCE)

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}" "${_BOOKWORM_CROSS_ROOT}" "${_BOOKWORM_CROSS_ROOT}/lib")
set(CMAKE_PREFIX_PATH "${_BOOKWORM_CROSS_ROOT}")
message(STATUS "Debian 12 AArch64 sysroot: ${CMAKE_SYSROOT}")
message(STATUS "Debian 12 AArch64 cross root: ${_BOOKWORM_CROSS_ROOT}")
message(STATUS "AArch64 C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "AArch64 C++ compiler: ${CMAKE_CXX_COMPILER}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -Wl,-rpath-link,${_BOOKWORM_CROSS_ROOT}/lib")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_SHARED_LINKER_FLAGS_INIT} -Wl,-rpath-link,${_BOOKWORM_CROSS_ROOT}/lib")
