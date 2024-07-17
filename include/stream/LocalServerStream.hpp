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

#include "boost/asio/local/stream_protocol.hpp"

#include "stream/LocalStream.hpp"

namespace daq::stream {
    class LocalServerStream : public LocalStream {
    public:
        LocalServerStream(boost::asio::local::stream_protocol::socket&& socket);
        LocalServerStream(const LocalServerStream&) = delete;
        LocalServerStream& operator= (LocalServerStream&) = delete;

        std::string endPointUrl() const override;
        std::string remoteHost() const override;
        void asyncInit(CompletionCb completionCb) override;
        boost::system::error_code init() override;
    };
}
