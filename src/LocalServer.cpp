#include <functional>

#include <boost/asio/local/stream_protocol.hpp>

#include "utils/syslog.h"
#include "stream/Stream.hpp"
#include "stream/LocalServer.hpp"
#include "stream/LocalServerStream.hpp"

namespace daq::stream {
    // placing a '\0' at the beginning of the endpoint name creates an abstract unix domain socket.
    // See man page (man 7 unix) for details
    static std::string getEndPointFileName(const std::string& localEndpointFile, bool useAbstractNamespace)
    {
        std::string endpointFileName;
        if (useAbstractNamespace) {
            endpointFileName = std::string("\0", 1);
        }
        endpointFileName += std::string(localEndpointFile);
        return endpointFileName;
    }

    LocalServer::LocalServer(boost::asio::io_context& readerIoContext, NewStreamCb newStreamCb, const std::string& localEndpointFile, bool useAbstractNamespace)
        : Server(newStreamCb)
        , m_localEndpointFile(localEndpointFile)
        , m_useAbstractNamespace(useAbstractNamespace)
        , m_localAcceptor(readerIoContext)
    {
    }

    LocalServer::~LocalServer()
    {
        stop();
    }

    int LocalServer::start()
    {
        if (!m_useAbstractNamespace) {
            // When not using abstract name space, try to remove the existing unix domain socket endpoint file.
            ::remove(m_localEndpointFile.c_str());
        }

        try {
            m_localAcceptor.open();
            m_localAcceptor.bind(getEndPointFileName(m_localEndpointFile, m_useAbstractNamespace));
            m_localAcceptor.listen();
            syslog(LOG_INFO, "Starting local server");
            startAccept();
            return 0;
        }  catch (const std::runtime_error& exc) {
            syslog(LOG_ERR, "Exception on start of local server: %s", exc.what());
            return -1;
        }
    }

    void LocalServer::stop()
    {
        syslog(LOG_INFO, "Stopping local server");
        m_localAcceptor.close();
        if (!m_useAbstractNamespace) {
            /// \todo When using non-abstract namespace, the endpoint file ist not being removed!
            ::remove(m_localEndpointFile.c_str());
        }
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
