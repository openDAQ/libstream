cmake_minimum_required(VERSION 3.13)

if (NOT CMAKE_MESSAGE_CONTEXT)
    set(CMAKE_MESSAGE_CONTEXT stream)
    set(CMAKE_MESSAGE_CONTEXT_SHOW ON CACHE BOOL "Show CMake message context")
endif()

project(stream VERSION 1.0.1 LANGUAGES CXX)

if (POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

get_filename_component(ROOT_DIR ${CMAKE_SOURCE_DIR} REALPATH)
if (NOT ${PROJECT_SOURCE_DIR} STREQUAL ${ROOT_DIR})
    set(BUILDING_AS_SUBMODULE ON)
    message(STATUS "Building as submodule")
else()
    message(STATUS "Building standalone")
    set(CMAKE_FOLDER "${CMAKE_FOLDER}/stream")
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER ".CMakePredefinedTargets")
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

include(StreamUtils)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (WIN32)
    set(MIN_SUPPORTED_WINDOWS 0x0601)
    add_compile_definitions(WIN32_LEAN_AND_MEAN
                            NOMINMAX
                            _WIN32_WINNT=${MIN_SUPPORTED_WINDOWS} # Windows 7 Compat
    )

    message(STATUS "Setting minimum supported API to Windows 7 (${MIN_SUPPORTED_WINDOWS})")
endif()

if (NOT COMMAND set_mode)
    message(STATUS "Including Modern.cmake")
    include(Modern)
else()
    set_mode(MODERN)
endif()

add_library(websocket INTERFACE)
add_library(daq::websocket ALIAS websocket)

target_compile_definitions(websocket INTERFACE BOOST_ALL_NO_LIB)

if (NOT TARGET Boost::beast)
    # we are using headers only (asio and beast for websockets)
    set(Boost_REQUIREDVERSION "1.71.0")
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_STATIC_LIBS ON)
    find_package(Boost ${Boost_REQUIREDVERSION} COMPONENTS system)

    if (Boost_FOUND)
        message(STATUS "Boost ${Boost_VERSION_STRING} at: ${Boost_INCLUDE_DIRS} | ${Boost_LIBRARY_DIRS}")

        target_link_libraries(websocket INTERFACE ${Boost_LIBRARIES})
        target_include_directories(websocket INTERFACE
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            $<BUILD_INTERFACE:${Boost_INCLUDE_DIRS}>
        )

        set(BOOST_LIBS ${Boost_LIBRARIES})
    else (Boost_FOUND)
        message(STATUS "Pre-installed required Boost not found. Fetching Boost 1.82.0 ...")
        get_custom_fetch_content_params(Boost FC_PARAMS)
        FetchContent_Declare(Boost
                             URL      https://github.com/boostorg/boost/releases/download/boost-1.82.0/boost-1.82.0.tar.xz
                             URL_HASH SHA256=fd60da30be908eff945735ac7d4d9addc7f7725b1ff6fcdcaede5262d511d21e
                             ${FC_PARAMS}
        )
        FetchContent_MakeAvailable(Boost)
    endif (Boost_FOUND)
endif()

if (TARGET Boost::beast)
    target_link_libraries(websocket
        INTERFACE
            $<BUILD_INTERFACE:Boost::beast>
            $<BUILD_INTERFACE:Boost::align>
    )
endif()

if (ENABLE_ASAN)
    set(ASAN_COMPILE_FLAGS -fsanitize=address -fno-omit-frame-pointer)
    set(ASAN_LINK_FLAGS asan)
endif()

add_subdirectory(external)
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
