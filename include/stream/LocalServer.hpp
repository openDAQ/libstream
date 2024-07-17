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
#include "boost/asio/local/stream_protocol.hpp"

#include "stream/Server.hpp"

namespace daq::stream {
    class LocalServer : public Server {
    public:       
        LocalServer(boost::asio::io_context& readerIoContext, NewStreamCb newStreamCb, const std::string &localEndpointFile);
        LocalServer(const LocalServer&) = delete;
        LocalServer& operator= (const LocalServer&) = delete;
        virtual ~LocalServer();
        int start();
        void stop();
    private:
        void startAccept();
        void onAccept(const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket&& streamSocket);
        
        std::string m_localEndpointFile;
        boost::asio::local::stream_protocol::acceptor m_localAcceptor;
    };
}
