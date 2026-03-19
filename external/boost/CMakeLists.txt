if (NOT TARGET Boost::beast)
    set(Boost_REQUIREDVERSION "1.71.0")
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_STATIC_LIBS ON)
    find_package(Boost ${Boost_REQUIREDVERSION} COMPONENTS system)

    if (Boost_FOUND)
        message(STATUS "Boost ${Boost_VERSION_STRING} at: ${Boost_INCLUDE_DIRS} | ${Boost_LIBRARY_DIRS}")
    else()
        message(STATUS "Pre-installed required Boost not found. Fetching Boost 1.82.0 ...")
        include(FetchContent)
        opendaq_get_custom_fetch_content_params(Boost FC_PARAMS)
        FetchContent_Declare(Boost
            URL      https://github.com/boostorg/boost/releases/download/boost-1.82.0/boost-1.82.0.tar.xz
            URL_HASH SHA256=fd60da30be908eff945735ac7d4d9addc7f7725b1ff6fcdcaede5262d511d21e
            ${FC_PARAMS}
        )
        FetchContent_MakeAvailable(Boost)
    endif()
endif()
