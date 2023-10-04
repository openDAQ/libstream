/* -*- Mode: c; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
#include <functional>
#include <iostream>

#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/ip/v6_only.hpp"

#include "utils/syslog.h"
#include "stream/Stream.hpp"
#include "stream/WebsocketServer.hpp"
#include "stream/WebsocketServerStream.hpp"
#include "stream/utils/boost_compatibility_utils.hpp"

using namespace boost::asio;

namespace daq::stream {
    WebsocketServer::WebsocketServer(boost::asio::io_context& readerIoContext, NewStreamCb newStreamCb, uint16_t tcpDataPort)
        : Server(newStreamCb)
        , m_tcpDataPort(tcpDataPort)
        , m_tcpAcceptorV4(readerIoContext)
        , m_tcpAcceptorV6(readerIoContext)
    {
    }
    
    WebsocketServer::~WebsocketServer()
    {
        stop();
    }
        
    int WebsocketServer::start()
    {
        syslog(LOG_INFO, "Starting websocket server");
    
        boost::system::error_code ec;
        m_tcpAcceptorV4.open(ip::tcp::v4(), ec);
        if (!ec)
        {
            m_tcpAcceptorV4.set_option(ip::tcp::acceptor::reuse_address(true));
            m_tcpAcceptorV4.bind(ip::tcp::endpoint(ip::tcp::v4(), m_tcpDataPort));
            m_tcpAcceptorV4.listen();
            startTcpAccept(m_tcpAcceptorV4);
        }
    
        m_tcpAcceptorV6.open(ip::tcp::v6(), ec);
        if (!ec)
        {
            m_tcpAcceptorV6.set_option(ip::v6_only(true));
            m_tcpAcceptorV6.set_option(ip::tcp::acceptor::reuse_address(true));
            m_tcpAcceptorV6.bind(ip::tcp::endpoint(ip::tcp::v6(), m_tcpDataPort));
            m_tcpAcceptorV6.listen();
            startTcpAccept(m_tcpAcceptorV6);
        }
    
        return 0;
    }
    
    void WebsocketServer::stop()
    {
        syslog(LOG_INFO, "Stopping websocket server");
        m_tcpAcceptorV4.close();
        m_tcpAcceptorV6.close();
    }

    void WebsocketServer::startTcpAccept(ip::tcp::acceptor& tcpAcceptor)
    {
        using namespace std::placeholders;
        tcpAcceptor.async_accept(std::bind(&WebsocketServer::onAccept, this, std::ref(tcpAcceptor), _1, _2));
    }

    void WebsocketServer::onAccept(ip::tcp::acceptor& tcpAcceptor,
                                   const boost::system::error_code& ec,
                                   boost::asio::ip::tcp::socket&& tcpSocket)
    {
        if (ec) {
            // also happens when stopping!
            return;
        }

        {
            std::shared_ptr<boost::beast::websocket::stream<boost::beast::tcp_stream>> websocket = std::make_shared<boost::beast::websocket::stream<boost::beast::tcp_stream>>(std::move(tcpSocket));
            websocket->write_buffer_bytes(65536);
            // Parameter websocket has to be passed per value to force another instance of the shared pointer!
            boost_compatibility_utils::async_accept(*websocket, [&, websocket](const boost::system::error_code& err)
            {
                onUpgrade(err, websocket);
            });
        }

        startTcpAccept(tcpAcceptor);
    }

    void WebsocketServer::onUpgrade(const boost::system::error_code& ec, std::shared_ptr < boost::beast::websocket::stream <boost::beast::tcp_stream > > websocket)
    {
        if (ec) {
            syslog(LOG_ERR, "Upgrade to websocket failed: %s", ec.message().c_str());
            return;
        }

        auto stream = std::make_shared < WebsocketServerStream > (websocket);

        auto initCb = [&, stream](const boost::system::error_code& ec)
        {
            if (ec) {
                syslog(LOG_ERR, "Websocket worker init failed: %s", ec.message().c_str());
                return;
            }
            try {
                m_newStreamCb(stream);
            } catch(...) {
                syslog(LOG_ERR, "Caught exception from init completion Cb!");
            }
        };
        stream->asyncInit(initCb);
    }
}
