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

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "Stream.hpp"

namespace daq::stream {
class WebsocketClientStream : public Stream
{

public:
    static const std::chrono::milliseconds DefaultConnectTimeout;

    /// Resolver and socket require an io_context
    /// @param path Used to reach serveral services on the same physical machine i.e. "/servicegreoup/service1". Must be at least "/".
    /// The complete URI of the service has the folowing form: <host>:<port><path>.
    explicit WebsocketClientStream(boost::asio::io_context& ioc, const std::string& host, const std::string &port, const std::string& path = "/");
    WebsocketClientStream(const WebsocketClientStream&) = delete;
    WebsocketClientStream& operator= (WebsocketClientStream&) = delete;


    /// Start the asynchronous operation of initializing the websocket connection.
    /// This includes address resolution, tcp connect and upgrade to websocket
    /// Uses default timeout DefaultConnectTimeout for tcp connect and websocket upgrade
    /// @param completionCb Executed after completion to start receiving/sending data
    void asyncInit(CompletionCb completionCb) override;
    /// @param initTimeout Maximum wait time for tcp connect and websocket upgrade
    void asyncInit(CompletionCb completionCb, std::chrono::milliseconds initTimeout);
    /// Synchronous operation of initializing the websocket connection.
    /// This includes address resolution, tcp connect and upgrade to websocket
    boost::system::error_code init() override;

    void asyncWrite(const boost::asio::const_buffer& data, Stream::WriteCompletionCb writeCompletionCb) override;
    void asyncWrite(const ConstBufferVector& data, WriteCompletionCb writeCompletionCb) override;
    size_t write(const boost::asio::const_buffer& data, boost::system::error_code& ec) override;
    size_t write(const ConstBufferVector& data, boost::system::error_code& ec) override;
    void asyncClose(CompletionCb closeCb) override;
    virtual boost::system::error_code close() override;


    std::string endPointUrl() const override;
    std::string remoteHost() const override;

private:
    void onResolve(const boost::beast::error_code& ec, boost::asio::ip::tcp::resolver::results_type results);
    void onConnect(const boost::beast::error_code& ec);
    void onUpgrade(const boost::beast::error_code& ec);

    void asyncReadAtLeast(std::size_t bytesToRead, ReadCompletionCb readAtLeastCb) override;
    size_t readAtLeast(std::size_t bytesToRead, boost::system::error_code &ec) override;
    void asyncTimeoutCb(const boost::system::error_code& ec);

    void setOptions();

    boost::asio::io_context& m_ioc;
    std::string m_host;
    std::string m_port;
    /// specifies the service on the server addressed with host and port
    std::string m_path;
    boost::beast::websocket::stream<boost::beast::tcp_stream> m_stream;
    boost::asio::ip::tcp::resolver m_resolver;
    boost::asio::deadline_timer m_asyncOperationTimer;
    std::chrono::milliseconds m_asyncTimeout;
};
}
