set_mode(ANCIENT)
set(CMAKE_FOLDER "external")

list(APPEND CMAKE_MESSAGE_CONTEXT external)

if (LIBSTREAM_POST_BUILD_UNITTEST)
    add_subdirectory(googletest)
endif()
