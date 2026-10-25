include(FetchContent)
set(SPDLOG_INSTALL ON CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG 6fa36017cfd5731d617e1a934f0e5ea9c4445b13
  OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(spdlog)
FetchContent_Declare(rapidcsv
  GIT_REPOSITORY https://github.com/d99kris/rapidcsv.git
  GIT_TAG 3c8131af7596329c50380ef3f8975e45742b1d60
  SOURCE_SUBDIR cge-header-only)
FetchContent_MakeAvailable(rapidcsv)
set(RAPIDCSV_INCLUDE_DIRS "${rapidcsv_SOURCE_DIR}/src" CACHE PATH "" FORCE)
FetchContent_Declare(commonlibsse
  GIT_REPOSITORY https://github.com/CharmedBaryon/CommonLibSSE-NG.git
  GIT_TAG b93280e832f263dbef44e44cbe2936622a02f91a)
FetchContent_MakeAvailable(commonlibsse)
target_compile_options(CommonLibSSE PRIVATE /Gy /Gw)

set_property(DIRECTORY "${commonlibsse_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)
set_property(DIRECTORY "${spdlog_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)
