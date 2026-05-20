if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES
                                          "GNU|gnu"
)
  set(_glibc_sysroot "")
  if(CMAKE_CROSSCOMPILING)
    set(_glibc_sysroot "${CMAKE_FIND_ROOT_PATH}")
  else()
    if(CMAKE_SYSROOT)
      set(_glibc_sysroot "${CMAKE_SYSROOT}")
    else()
      execute_process(
        COMMAND ${CMAKE_C_COMPILER} -print-sysroot
        OUTPUT_VARIABLE _glibc_sysroot
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    endif()
  endif()

  message(STATUS "Determined sysroot for glibc check: ${_glibc_sysroot}")
  set(_glibc_check_c "${CMAKE_BINARY_DIR}/cmake_glibc_check.c")
  file(WRITE "${_glibc_check_c}" "#include <features.h>\n")
  execute_process(
    COMMAND ${CMAKE_C_COMPILER} --sysroot=${_glibc_sysroot} -E -dM -x c
            "${_glibc_check_c}"
    OUTPUT_VARIABLE _glibc_macros
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message(STATUS "Checking glibc version from sysroot: ${_glibc_sysroot}")
  string(REGEX MATCH "#define[ \t]+__GLIBC__[ \t]+([0-9]+)" _m1
               "${_glibc_macros}"
  )
  set(_glibc_major "${CMAKE_MATCH_1}")
  string(REGEX MATCH "#define[ \t]+__GLIBC_MINOR__[ \t]+([0-9]+)" _m2
               "${_glibc_macros}"
  )
  set(_glibc_minor "${CMAKE_MATCH_1}")
  message(
    STATUS "Parsed glibc version: major=${_glibc_major}, minor=${_glibc_minor}"
  )

  if(_glibc_major AND _glibc_minor)
    set(GLIBC_VERSION "${_glibc_major}.${_glibc_minor}")
    message(
      STATUS
        "Detected target glibc version: ${GLIBC_VERSION} (sysroot=${_glibc_sysroot})"
    )
  else()
    message(
      WARNING
        "Failed to detect target glibc version from sysroot macros (sysroot=${_glibc_sysroot})"
    )
  endif()
else()
  message(
    STATUS
      "Not running on Linux with GNU compiler, skipping glibc version check"
  )
  message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
  message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
endif()
