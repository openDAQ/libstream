cmake_minimum_required(VERSION 3.13)

set(INTERFACE_HEADERS
    Stream.hpp
    Server.hpp
    TcpClientStream.hpp
    TcpServerStream.hpp
    TcpServer.hpp
    WebsocketClientStream.hpp
    WebsocketServerStream.hpp
    WebsocketServer.hpp
    utils/boost_compatibility_utils.hpp
)

if (NOT WIN32)
    set(INTERFACE_HEADERS
        ${INTERFACE_HEADERS}
        FileStream.hpp
        LocalStream.hpp
        LocalClientStream.hpp
        LocalServer.hpp
        LocalServerStream.hpp
    )
endif()

prepend_include(${PROJECT_NAME} INTERFACE_HEADERS)

set(LIB_SOURCES
    ${INTERFACE_HEADERS}
    Stream.cpp
    TcpStream.cpp
    TcpClientStream.cpp
    TcpServer.cpp
    TcpServerStream.cpp
    WebsocketClientStream.cpp
    WebsocketServerStream.cpp
    WebsocketServer.cpp
    utils/boost_compatibility_utils.cpp
)

# Windows does not support UNIX domain sockets
# WIN32 includes 64 bit architecture
if (NOT WIN32)
    set(LIB_SOURCES
            ${LIB_SOURCES}
            FileStream.cpp
            LocalClientStream.cpp
            LocalServer.cpp
            LocalServerStream.cpp
	    LocalStream.cpp
    )
endif()

if(MSVC)
    if(${MSVC_VERSION} LESS 1920)
        # msvc-17 has a compiler bug when cmpiling functions defined in boost_compatibility_utils
        # this is why we compile them using cxx_std_14
        set_source_files_properties(
            utils/boost_compatibility_utils.cpp
            PROPERTIES COMPILE_FLAGS /std:c++14
        )
    endif()
endif()

add_library(${PROJECT_NAME} STATIC ${LIB_SOURCES})
add_library(daq::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# required if this library will ever be linked into a shared object
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/>
)

target_link_libraries(${PROJECT_NAME} PUBLIC ${CMAKE_THREAD_LIBS_INIT}
                                             daq::websocket
                                             ${Boost_LIBRARIES}
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)

    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
		target_compile_options(${PROJECT_NAME} PRIVATE /WX-)
    elseif(CMAKE_COMPILER_IS_GNUCXX)
        target_compile_options(${PROJECT_NAME} PRIVATE -Wa,-mbig-obj)
    endif()
endif()
