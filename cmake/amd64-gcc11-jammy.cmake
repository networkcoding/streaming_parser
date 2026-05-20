set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
# sudo debootstrap --variant=minbase --include=gcc-11,g++-11,libbsd-dev \
#                  --arch=amd64 jammy /opt/sysroots/jammy https://mirrors.tuna.tsinghua.edu.cn/ubuntu
set(CMAKE_SYSROOT /opt/sysroots/jammy)

# Host tools (binutils)
# A debootstrap rootfs is a runtime/sysroot, not a standalone external toolchain.
if(NOT EXISTS "/usr/bin/gcc-11" OR NOT EXISTS "/usr/bin/g++-11")
  message(FATAL_ERROR "Host /usr/bin/gcc-11 and /usr/bin/g++-11 are required. Install them first, e.g. `sudo apt install gcc-11 g++-11`.")
  # Gcc management
  # sudo apt install --yes gcc-11 g++-11
  # sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 11
  # sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 11
  # sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-15 15
  # sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-15 15
  # sudo update-alternatives --set gcc /usr/bin/gcc-11
  # sudo update-alternatives --set g++ /usr/bin/g++-11
  # update-alternatives --list gcc
endif()

set(CMAKE_C_COMPILER /usr/bin/gcc-11)
set(CMAKE_CXX_COMPILER /usr/bin/g++-11)
set(CMAKE_AR /usr/bin/ar CACHE FILEPATH "Host archiver" FORCE)
set(CMAKE_RANLIB /usr/bin/ranlib CACHE FILEPATH "Host ranlib" FORCE)
set(CMAKE_NM /usr/bin/nm CACHE FILEPATH "Host nm" FORCE)
set(CMAKE_LINKER /usr/bin/ld CACHE FILEPATH "Host linker" FORCE)
if(EXISTS "/usr/bin/strip")
  set(CMAKE_STRIP /usr/bin/strip CACHE FILEPATH "Host strip" FORCE)
endif()
if(EXISTS "/usr/bin/objcopy")
  set(CMAKE_OBJCOPY /usr/bin/objcopy CACHE FILEPATH "Host objcopy" FORCE)
endif()

set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
message(STATUS "ROOT PATH: ${CMAKE_FIND_ROOT_PATH}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
