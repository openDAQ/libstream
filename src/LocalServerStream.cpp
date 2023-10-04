#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>


#include "stream/LocalServerStream.hpp"

namespace daq::stream {
    LocalServerStream::LocalServerStream(boost::asio::local::stream_protocol::socket&& socket)
        : LocalStream(std::move(socket))
    {
    }

    void LocalServerStream::asyncInit(CompletionCb completionCb)
    {
	boost::system::error_code ec = init();
        completionCb(ec);
    }

    boost::system::error_code LocalServerStream::init()
    {
        return boost::system::error_code();
    }

    std::string LocalServerStream::endPointUrl() const
    {
        return "";//m_endpointFile;
    }

    std::string LocalServerStream::remoteHost() const
    {
        return "";
    }
}
