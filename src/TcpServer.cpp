#include <functional>

#include "boost/asio/ip/tcp.hpp"

#include "utils/syslog.h"
#include "stream/Stream.hpp"
#include "stream/TcpServer.hpp"
#include "stream/TcpServerStream.hpp"

namespace daq::stream {
    /// Maximum number of sessions allowed
    //static const unsigned int maxWorkerCount = 8;
    
    TcpServer::TcpServer(boost::asio::io_context& readerIoContext, NewStreamCb newStreamCb, uint16_t tcpDataPort)
        : Server(newStreamCb)
        , m_tcpDataPort(tcpDataPort)
        , m_tcpAcceptor(readerIoContext, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), m_tcpDataPort))
    {
    }
    
    TcpServer::~TcpServer()
    {
        stop();
    }
        
    int TcpServer::start()
    {
        syslog(LOG_INFO, "Starting tcp server");
        startTcpAccept();
        return 0;
    }
    
    void TcpServer::stop()
    {
        syslog(LOG_INFO, "Stopping tcp server");
        m_tcpAcceptor.close();
    }

    void TcpServer::startTcpAccept()
    {
        auto handlTcpAccept = [this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket&& streamSocket) {
            if (ec) {
                // also happens when stopping!
                return;
            }
            // here we create a new stream and initialize it. Afterwards we call a callback function to provide the error code and the stream itself.
            auto stream = std::make_shared < TcpServerStream > (std::move(streamSocket));
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
            startTcpAccept();
        };
        m_tcpAcceptor.async_accept(handlTcpAccept);
    }
}
