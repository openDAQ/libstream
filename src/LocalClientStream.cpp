#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>


#include "stream/LocalClientStream.hpp"

namespace daq::stream {
    LocalClientStream::LocalClientStream(boost::asio::io_context& ioc, const std::string &endPointFile)
        : LocalStream(ioc)
        , m_ioc(ioc)
        , m_endpointFile(endPointFile)
    {
    }

    void LocalClientStream::asyncInit(CompletionCb completionCb)
    {
        m_initCompletionCb = completionCb;
        if (m_socket.is_open()) {
            auto completionCb = [this]()
            {
                m_initCompletionCb(boost::system::error_code());
            };
            m_ioc.dispatch(completionCb);
            return;
        }

        // placing a '\0' at the beginning of the endpoint name creates an abstract unix domain socket.
        // See man page (man 7 unix) for details
        m_socket.async_connect(
                    boost::asio::local::stream_protocol::endpoint(std::string("\0", 1) + std::string(m_endpointFile)),
                    std::bind(m_initCompletionCb, std::placeholders::_1));
    }

    boost::system::error_code LocalClientStream::init()
    {
        if (m_socket.is_open()) {
            return boost::system::error_code();
        }
        boost::system::error_code ec;
        // placing a '\0' at the beginning of the endpoint name creates an abstract unix domain socket.
        // See man page (man 7 unix) for details
        m_socket.connect(
                    boost::asio::local::stream_protocol::endpoint(std::string("\0", 1) + std::string(m_endpointFile)),
                    ec);
        return ec;
    }

    std::string LocalClientStream::endPointUrl() const
    {
        return m_endpointFile;
    }

    std::string LocalClientStream::remoteHost() const
    {
        // This is used as the address to send subscribe/unsubscribe http post to
        return "";
    }
}

