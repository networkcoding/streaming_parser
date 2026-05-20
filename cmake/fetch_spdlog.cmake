if(NOT TARGET spdlog::spdlog)
  message(STATUS "spdlog not found, fetching spdlog...")
  FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    GIT_SHALLOW_DEPTH 1
    FETCHCONTENT_QUIET FALSE FIND_PACKAGE_ARGS
  )
  FetchContent_MakeAvailable(spdlog)
  include_directories(${spdlog_SOURCE_DIR}/include)
else()
  message(STATUS "spdlog found, using existing spdlog...")
endif()
message(STATUS "spdlog include directories: ${spdlog_SOURCE_DIR}/include")
