# target_link_options requires cmake >= 3.13
cmake_minimum_required(VERSION 3.13)
set_cmake_folder_context(TARGET_FOLDER_NAME)
project(libstream-test CXX)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CGOV_COMPILE_FLAGS -fno-omit-frame-pointer -fno-optimize-sibling-calls -ftest-coverage -fprofile-arcs)
  set(GCOV_LINK_FLAGS -fprofile-arcs -ftest-coverage)
  set(GCOV_LIB gcov)

  if (NOT MINGW)
    set(ASAN_COMPILE_FLAGS -fsanitize=address -fno-omit-frame-pointer)
    set(ASAN_LIB asan)
  endif()
endif()

add_custom_target(coverage
  COMMAND echo "=================== Run ===================="
  COMMAND ${CMAKE_MAKE_PROGRAM} test ARGS="--output-on-failure"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set_target_properties(coverage
  PROPERTIES
  ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/*.html"
)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES coverage)

enable_testing()

set(TEST_LIB_SOURCES
    ../src/Stream.cpp
    ../src/TcpStream.cpp
    ../src/TcpClientStream.cpp
    ../src/TcpServer.cpp
    ../src/TcpServerStream.cpp
    ../src/utils/boost_compatibility_utils.cpp
    ../src/WebsocketClientStream.cpp
    ../src/WebsocketServer.cpp
    ../src/WebsocketServerStream.cpp
)

# Windows does not support:
# -UNIX domain sockets
# -posix streams which are used in FileStream
if (NOT WIN32)
    set(TEST_LIB_SOURCES
        ${TEST_LIB_SOURCES}
        ../src/FileStream.cpp
        ../src/LocalStream.cpp
        ../src/LocalClientStream.cpp
        ../src/LocalServer.cpp
        ../src/LocalServerStream.cpp
    )
endif()

set(STREAM_TEST_LIB stream_test_lib)
add_library(${STREAM_TEST_LIB} OBJECT ${TEST_LIB_SOURCES})
target_include_directories(${STREAM_TEST_LIB} PUBLIC ${Boost_INCLUDE_DIRS})
target_compile_options(${STREAM_TEST_LIB} PRIVATE
 $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
  -Wall -Werror ${CGOV_COMPILE_FLAGS} ${ASAN_COMPILE_FLAGS}>
)

target_compile_definitions(${STREAM_TEST_LIB} PRIVATE
    _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
)

target_include_directories(${STREAM_TEST_LIB} PRIVATE ../include)

set_target_properties(${STREAM_TEST_LIB} PROPERTIES
  CXX_STANDARD_REQUIRED ON
  CXX_STANDARD 20
  CXX_EXTENSIONS OFF
)

target_include_directories(stream_test_lib PRIVATE ../include)
target_link_options(${STREAM_TEST_LIB} PRIVATE ${GCOV_LINK_FLAGS})
target_link_libraries(
  ${STREAM_TEST_LIB}
  PUBLIC ${ASAN_LIB}
         ${GCOV_LIB}
         ${Boost_LIBRARIES}
)

# Windows does not support UNIX domain sockets
if (NOT WIN32)
    add_executable( FileStream.test FileStreamTest.cpp)
    add_executable( LocalStream.test LocalStreamTest.cpp)
endif()
add_executable( Stream.test StreamTest.cpp)
add_executable( TcpStream.test TcpStreamTest.cpp)
add_executable( WebsocketStream.test WebsocketStreamTest.cpp)

if (WIN32)
  target_link_libraries(${STREAM_TEST_LIB}
      PUBLIC ws2_32
             wsock32
  )
endif()

get_property(targets DIRECTORY "${CMAKE_CURRENT_LIST_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
foreach(tgt ${targets})
  get_target_property(target_type ${tgt} TYPE)
  if (target_type STREQUAL "EXECUTABLE")
    target_include_directories(${tgt} PRIVATE ../include)
    target_compile_definitions(${tgt} PRIVATE _FB_SIMULATION)
    target_compile_options(${tgt} PRIVATE
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
          -Wall -Werror ${CGOV_COMPILE_FLAGS} ${ASAN_COMPILE_FLAGS}>
    )
    # requires cmake 3.13
    target_link_options(${tgt} PRIVATE ${GCOV_LINK_FLAGS} )
    target_link_libraries(
      ${tgt}
      ${ASAN_LIB}
      ${GCOV_LIB}
      GTest::GTest GTest::Main
      ${STREAM_TEST_LIB}
    )

    set_target_properties(${tgt} PROPERTIES
      CXX_STANDARD_REQUIRED ON
      CXX_STANDARD 20
      CXX_EXTENSIONS OFF
    )
    add_test(NAME libstream.${tgt} COMMAND ${tgt})
    add_dependencies(coverage ${tgt})

  endif()
endforeach()

set(COMMON_BRANCH_OPTIONS "--exclude-unreachable-branches" "--exclude-throw-branches")
# exclude tests and external library code form coverage
# note: cmake replaces ' ' in string with '\ ' creating a list solves this problem; add --branches to use branch coverage again
set(EXCLUDED_DIRECTORIES "-e${CMAKE_SOURCE_DIR}/test" "-e${CMAKE_BINARY_DIR}/_deps")

add_custom_command(TARGET coverage
  COMMAND gcovr -r ${CMAKE_SOURCE_DIR} ${EXCLUDED_DIRECTORIES} --object-dir=${CMAKE_BINARY_DIR} ${COMMON_BRANCH_OPTIONS} --html-details -o ${CMAKE_BINARY_DIR}/coverage.html
  COMMAND gcovr -r ${CMAKE_SOURCE_DIR} ${EXCLUDED_DIRECTORIES} --object-dir=${CMAKE_BINARY_DIR} ${COMMON_BRANCH_OPTIONS} --json -o ${CMAKE_BINARY_DIR}/coverage.json
  COMMAND gcovr -r ${CMAKE_SOURCE_DIR} ${EXCLUDED_DIRECTORIES} --object-dir=${CMAKE_BINARY_DIR} ${COMMON_BRANCH_OPTIONS}
  )
