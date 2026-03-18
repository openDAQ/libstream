cmake_minimum_required(VERSION 3.24)

if (NOT CMAKE_MESSAGE_CONTEXT)
    set(CMAKE_MESSAGE_CONTEXT stream)
    set(CMAKE_MESSAGE_CONTEXT_SHOW ON CACHE BOOL "Show CMake message context")
endif()

project(stream VERSION 1.0.4 LANGUAGES CXX)

add_subdirectory(cmake)

if (POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

get_filename_component(ROOT_DIR ${CMAKE_SOURCE_DIR} REALPATH)

if (PROJECT_IS_TOP_LEVEL)
    message(STATUS "Building standalone")
    set(CMAKE_FOLDER "${CMAKE_FOLDER}/stream")
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER ".CMakePredefinedTargets")
else()
    message(STATUS "Building as submodule")
endif()

option(LIBSTREAM_POST_BUILD_UNITTEST  "Automatically run unit-tests as a post build step" OFF)

if (MINGW AND CMAKE_COMPILER_IS_GNUCXX)
    message(WARNING "Address sanitizer is not supported under MinGW GCC")
else()
    option(ENABLE_ASAN "Build with address sanitizer" OFF)
endif()

find_package(Threads)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake
)

set(FETCHCONTENT_EXTERNALS_DIR ${ROOT_DIR}/build/__external CACHE PATH "FetchContent folder prefix")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

opendaq_get_cmake_mode(_LIBSTREAM_CMAKE_MODE)
if (NOT _LIBSTREAM_CMAKE_MODE)
    opendaq_set_cmake_mode(MODERN)
endif()

if (WIN32)
    set(MIN_SUPPORTED_WINDOWS 0x0601)
    add_compile_definitions(WIN32_LEAN_AND_MEAN
                            NOMINMAX
                            _WIN32_WINNT=${MIN_SUPPORTED_WINDOWS} # Windows 7 Compat
    )

    message(STATUS "Setting minimum supported API to Windows 7 (${MIN_SUPPORTED_WINDOWS})")
endif()

add_library(websocket INTERFACE)
add_library(daq::websocket ALIAS websocket)

target_compile_definitions(websocket INTERFACE BOOST_ALL_NO_LIB)

if (ENABLE_ASAN)
    set(ASAN_COMPILE_FLAGS -fsanitize=address -fno-omit-frame-pointer)
    set(ASAN_LINK_FLAGS asan)
endif()

add_subdirectory(external)

target_link_libraries(websocket
    INTERFACE
        $<BUILD_INTERFACE:Boost::beast>
        $<BUILD_INTERFACE:Boost::align>
)

add_subdirectory(src)

# install the library and generate export set (note that no
# targets file is generated or installed in this step)
install(
    TARGETS ${PROJECT_NAME} websocket
    EXPORT ${PROJECT_NAME}
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

# install the public headers
install(
    DIRECTORY include/${PROJECT_NAME}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# install a targets file for the generated export set
install(
    EXPORT ${PROJECT_NAME}
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE daq::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${${PROJECT_NAME}_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

if (LIBSTREAM_POST_BUILD_UNITTEST)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CGOV_COMPILE_FLAGS -fno-omit-frame-pointer -fno-optimize-sibling-calls -ftest-coverage -fprofile-arcs)
        set(GCOV_LINK_FLAGS -fprofile-arcs -ftest-coverage)
        set(GCOV_LIB gcov)
    endif()

    enable_testing()
    add_subdirectory(test)
endif(LIBSTREAM_POST_BUILD_UNITTEST)
