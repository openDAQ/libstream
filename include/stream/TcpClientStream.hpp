/*
 * Copyright 2022-2023 Blueberry d.o.o.
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

#include <string>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "stream/TcpStream.hpp"

namespace daq::stream {
    class TcpClientStream : public TcpStream
    {
    public:
        static const std::chrono::milliseconds DefaultConnectTimeout;

        explicit TcpClientStream(boost::asio::io_context& ioc, const std::string& host, const std::string& port);
        TcpClientStream(const TcpClientStream&) = delete;
        TcpClientStream& operator= (const TcpClientStream&) = delete;
        ~TcpClientStream() = default;

        /// Start the asynchronous operation
        /// \param completionCb Will be called on completion with the result.
        /// On connection timeout error code will be boost::system::errc::operation_canceled
        void asyncInit(CompletionCb completionCb) override;
        /// @param connectTimeout Maximum wait time for connect.
        void asyncInit(CompletionCb completionCb, std::chrono::milliseconds connectTimeout);
        /// Synchronous operation of initializing the websocket connection.
        /// This includes address resolution, tcp connect and upgrade to websocket
        boost::system::error_code init() override;

        std::string endPointUrl() const override;
        std::string remoteHost() const override;


    private:
        /// Timed out after 5 seconds
        void onResolve(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results);
        void onConnect(const boost::system::error_code& ec);

        void connectTimeoutCb(const boost::system::error_code& ec);

        boost::asio::io_context& m_ioc;
        std::string m_host;
        std::string m_port;
        boost::asio::ip::tcp::resolver m_resolver;
        boost::asio::deadline_timer m_connectTimer;
        std::chrono::milliseconds m_connectTimeout;
    };
}

