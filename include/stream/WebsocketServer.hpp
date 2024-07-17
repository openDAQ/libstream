/*
 * Copyright 2022-2024 openDAQ d.o.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once


#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"


#include "stream/Server.hpp"
#include "stream/Stream.hpp"
#include "stream/WebsocketServerStream.hpp"

namespace daq::stream {
    class WebsocketServer : public Server {
    public:
        /// \param tcpDataPort Using a port <= 1024 causes bind error when not having root rights
        /// \throw std::runtime_error on bind error
        WebsocketServer(boost::asio::io_context& readerIoContext, NewStreamCb newStreamCb, uint16_t tcpDataPort);
        WebsocketServer(const WebsocketServer&) = delete;
        WebsocketServer& operator= (const WebsocketServer&) = delete;
        virtual ~WebsocketServer();
        int start();
        void stop();
    private:
        void startTcpAccept(boost::asio::ip::tcp::acceptor& tcpAcceptor);
        void onAccept(boost::asio::ip::tcp::acceptor& tcpAcceptor,
                      const boost::system::error_code& ec,
                      boost::asio::ip::tcp::socket&& tcpSocket);
        /// called after completion of upgrade to websocket
        void onUpgrade(const boost::system::error_code& ec, std::shared_ptr < boost::beast::websocket::stream <boost::beast::tcp_stream > > websocket);
        
        uint16_t m_tcpDataPort;
        boost::asio::ip::tcp::acceptor m_tcpAcceptorV4;
        boost::asio::ip::tcp::acceptor m_tcpAcceptorV6;
    };
}
