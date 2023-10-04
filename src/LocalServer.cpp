#include <functional>

#include <boost/asio/local/stream_protocol.hpp>

#include "utils/syslog.h"
#include "stream/Stream.hpp"
#include "stream/LocalServer.hpp"
#include "stream/LocalServerStream.hpp"

namespace daq::stream {
    LocalServer::LocalServer(boost::asio::io_context& readerIoContext, NewStreamCb newStreamCb, const std::string& localEndpointFile)
        : Server(newStreamCb)
        , m_localEndpointFile(localEndpointFile)
        , m_localAcceptor(readerIoContext, std::string("\0", 1) + std::string(localEndpointFile))
    {
    }


    
    LocalServer::~LocalServer()
    {
        stop();
    }
    
    
    int LocalServer::start()
    {
        syslog(LOG_INFO, "Starting local server");
        startAccept();
        return 0;
    }
    
    void LocalServer::stop()
    {
        syslog(LOG_INFO, "Stopping local server");
        m_localAcceptor.close();
    }

    

    void LocalServer::startAccept()
    {
        m_localAcceptor.async_accept(std::bind(&LocalServer::onAccept, this, std::placeholders::_1, std::placeholders::_2));
    }

    void LocalServer::onAccept(const boost::system::error_code &ec, boost::asio::local::stream_protocol::socket &&streamSocket)
    {
        if (ec) {
            // also happens when stopping!
            return;
        }
        // A new stream is created and initialized asynchronously. On completion the final callback provides the error code and the stream itself.
        auto stream = std::make_shared < LocalServerStream > (std::move(streamSocket));
        auto completionCb = [&, stream](const boost::system::error_code& ec)
        {
            if(ec) {
                syslog(LOG_ERR, "async init failed: %s", ec.message().c_str());
                return;
            }
            try {
                m_newStreamCb(stream);
            } catch(...) {
                syslog(LOG_ERR, "Caught exception from init completion Cb!");
            }
        };
        stream->asyncInit(completionCb);
        startAccept();
    }
}
