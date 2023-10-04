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

#include "boost/asio/local/stream_protocol.hpp"

#include "stream/Stream.hpp"

namespace daq::stream {
    class LocalStream : public Stream {
    public:
        LocalStream(boost::asio::io_context& ioc);
        LocalStream(boost::asio::local::stream_protocol::socket&& socket);
        LocalStream(const LocalStream&) = delete;
        LocalStream& operator= (LocalStream&) = delete;
        
        
        void asyncWrite(const boost::asio::const_buffer& data, WriteCompletionCb writeCompletionCb) override;
        void asyncWrite(const ConstBufferVector& data, WriteCompletionCb writeCompletionCb) override;
        size_t write(const boost::asio::const_buffer& data, boost::system::error_code& ec) override;
        size_t write(const ConstBufferVector& data, boost::system::error_code& ec) override;

        void asyncClose(CompletionCb closeCb) override;
        boost::system::error_code close() override;

    protected:
        void asyncReadAtLeast(std::size_t bytesToRead, ReadCompletionCb readAtLeastCb) override;
        size_t readAtLeast(std::size_t bytesToRead, boost::system::error_code &ec) override;

        boost::asio::local::stream_protocol::socket m_socket;
    };
}
