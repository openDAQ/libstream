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

#ifdef _WIN32
#error The current implementation for windows is not working correctly. When reading from file, second read returns the same data as the first read
#include "boost/asio/windows/stream_handle.hpp"
#else
#include "boost/asio/posix/stream_descriptor.hpp"
#endif

#include "Stream.hpp"

namespace daq::stream {
    /// Can be used to read from/write to file or character device file.
    /// It operates on an existing file
    class FileStream : public Stream
    {

    public:
        /// \param fileName Existing file to open
        explicit FileStream(boost::asio::io_context& ioc, const std::string& fileName, bool writable = false);
        FileStream(const FileStream&) = delete;
        FileStream& operator= (FileStream&) = delete;

        /// Start the asynchronous operation
        void asyncInit(CompletionCb completionCb) override;
        boost::system::error_code init() override;
        void asyncWrite(const boost::asio::const_buffer& data, WriteCompletionCb writeCompletionCb) override;
        void asyncWrite(const ConstBufferVector& data, WriteCompletionCb writeCompletionCb) override;

        size_t write(const boost::asio::const_buffer& data, boost::system::error_code& ec) override;
        size_t write(const ConstBufferVector& data, boost::system::error_code& ec) override;

        void asyncClose(CompletionCb closeCb) override;
        boost::system::error_code close() override;

        std::string endPointUrl() const override;
        std::string remoteHost() const override;

    private:
        void asyncReadAtLeast(std::size_t bytesToRead, ReadCompletionCb readCompletionCb) override;
        size_t readAtLeast(std::size_t bytesToRead, boost::system::error_code &ec) override;

        boost::asio::io_context& m_ioc;
        std::string m_fileName;
#ifdef _WIN32
        boost::asio::windows::stream_handle m_fileStream;
#else
        boost::asio::posix::stream_descriptor m_fileStream;
#endif
        bool m_writable;
    };
}
