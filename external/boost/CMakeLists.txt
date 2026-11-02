# handle Boost dependencies here only if not already handled by parent project
if (NOT TARGET Boost::beast)
    opendaq_append_required_boost_components(
        asio
        beast
        align
    )
    opendaq_complete_boost_dependency(
        VERSION     "1.92.0"
        URL         "https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-cmake.tar.gz"
        URL_HASH    "SHA256=f51707c27359a0df0cac1beada86de31bb5eed5e8285592dadec384df99c2984"
    )
endif()
