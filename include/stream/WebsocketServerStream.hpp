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

#include <string>

#include "boost/asio/buffer.hpp"
#include <boost/beast/core/tcp_stream.hpp>

#include "boost/beast/websocket/stream.hpp"

#include "Stream.hpp"

namespace daq::stream {
    class WebsocketServerStream : public Stream {
    public:
        /// websocket upgrade will happen later in asyncInit()
        WebsocketServerStream(std::shared_ptr<boost::beast::websocket::stream<boost::beast::tcp_stream> > websocket);
        WebsocketServerStream(const WebsocketServerStream&) = delete;
        WebsocketServerStream& operator= (WebsocketServerStream&) = delete;

        std::string endPointUrl() const override;
        std::string remoteHost() const override;
        /// Upgrade to websocket happens here.
        void asyncInit(CompletionCb completionCb) override;
        boost::system::error_code init() override;
        void asyncWrite(const boost::asio::const_buffer& data, WriteCompletionCb writeCompletionCb) override;
        void asyncWrite(const ConstBufferVector& data, Stream::WriteCompletionCb writeCompletionCb) override;
        size_t write(const boost::asio::const_buffer& data, boost::system::error_code& ec) override;
        size_t write(const ConstBufferVector& data, boost::system::error_code& ec) override;

        void asyncClose(CompletionCb closeCb) override;
        boost::system::error_code close() override;
    private:
        /// websocket accept (handshake)
        void onAccept(const boost::beast::error_code& ec);
        void asyncReadAtLeast(std::size_t bytesToRead, ReadCompletionCb readAtLeastCb) override;
        size_t readAtLeast(std::size_t bytesToRead, boost::system::error_code &ec) override;
        void setOptions();
        std::shared_ptr<boost::beast::websocket::stream<boost::beast::tcp_stream> > m_websocket;
    };
}
