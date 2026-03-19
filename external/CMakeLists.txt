opendaq_get_cmake_mode(_LIBSTREAM_CMAKE_MODERN_MODE_SAVED)
opendaq_set_cmake_mode(ANCIENT)
set(CMAKE_FOLDER "external")
list(APPEND CMAKE_MESSAGE_CONTEXT external)

add_subdirectory(boost)

if (LIBSTREAM_POST_BUILD_UNITTEST)
    add_subdirectory(googletest)
endif()

opendaq_set_cmake_mode(${_LIBSTREAM_CMAKE_MODERN_MODE_SAVED})
