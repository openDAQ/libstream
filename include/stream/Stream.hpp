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

#include <vector>

#include <boost/asio/streambuf.hpp>
#include <boost/system/error_code.hpp>


namespace daq::stream {
    using ConstBufferVector = std::vector <boost::asio::const_buffer>;

    /// Base class of a stream.
    /// Used for receiving many small data bits efficiently from and writing to a stream.
    ///
    /// Efficient reading of small packets is done by allowing to receive and buffer more data than actually requested.
    /// Additional data is kept in a buffer until requested by the next read request.
    /// There exist derived classes that implement this for TCP, Websocket, Unix Domain socket and file access.
    ///
    /// Stream provides information about the remote endpoint.
    class Stream
    {
    public:
        /// @param bytesRead Number of bytes acually read. Might be bigger than requested.
        using ReadCompletionCb = std::function <void (const boost::system::error_code& ec, std::size_t bytesRead) >;
        using WriteCompletionCb = std::function <void (const boost::system::error_code& ec, std::size_t bytesWritten) >;
        using CompletionCb = std::function <void (const boost::system::error_code& ec) >;

        virtual ~Stream() = default;

        /// Initialize depending on the kind of stream
        /// @param CompletionCb Executed after completion to start reading/writing data
        virtual void asyncInit(CompletionCb competionCb) = 0;
        virtual boost::system::error_code init() = 0;

        /// \return Url of the remote endpoint depending on session type
        virtual std::string endPointUrl() const = 0;
        /// \return Hostname/address of the remote endpoint. Empty for localhost.
        virtual std::string remoteHost() const = 0;

        /// Request to read an amount of data. On completion, callback function is executed.
        /// A receive buffer is used that allows to read more data than requested. Additional data being read is kept for the next read request.
        /// @warning If the requested amount of data is already available, callback function is executed directly in the same context.
        /// When having lots of data in the buffer, containing lots of small packages, we might have lots of method calls resulting in a very deep stack.
        /// @param readCb Processing callback to be executed upon data availability
        void asyncRead(CompletionCb readCb, std::size_t size);
        boost::system::error_code read(std::size_t size);
        /// Read at least one byte
        /// If there is data remaining in the buffer, readCb is called directly. Otherwise an asynchronous read of at least one byte is started.
        void asyncReadSome(ReadCompletionCb readCb);
        size_t readSome(boost::system::error_code& ec);
        virtual void asyncWrite(const boost::asio::const_buffer& dataBuffer, WriteCompletionCb writeCompletionCb) = 0;
        /// Sending a sequence of buffers is usefull to avoid copying parts into one memory area before sending.
        virtual void asyncWrite(const ConstBufferVector& data, WriteCompletionCb writeCompletionCb) = 0;

        virtual size_t write(const boost::asio::const_buffer& data, boost::system::error_code& ec) = 0;
        virtual size_t write(const ConstBufferVector& data, boost::system::error_code& ec) = 0;

        /// This operation is asnychronous because there is a closing handshake for websockets.
        virtual void asyncClose(CompletionCb closeCb) = 0;
        virtual boost::system::error_code close() = 0;

        /// \return Amount of consumable (available) data
        size_t size() const;
        /// Get direct access to the available, consumable (available) data in the buffer
        const uint8_t* data() const;
        /// Consume available data. "Move the read pointer of the buffer by size".
        /// If there is not enough data available, the amount of available data is cosumed
        void consume(size_t size);

        /// Copies data from buffer to dest and consumes it afterwards.
        /// @warning No check whether enough is available
        void copyDataAndConsume(void* dest, size_t size);

    protected:
        virtual void asyncReadAtLeast(std::size_t bytesToRead, ReadCompletionCb readCompletionCb) = 0;
        virtual size_t readAtLeast(std::size_t bytesToRead, boost::system::error_code &ec) = 0;

        /// will be called upon completion of asyncInit
        CompletionCb m_initCompletionCb;
        boost::asio::streambuf m_buffer;
    };
}
