#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include "stream/LocalStream.hpp"

namespace daq::stream {
    LocalStream::LocalStream(boost::asio::io_context& ioc)
        : m_socket(ioc)
    {
    }
    
    LocalStream::LocalStream(boost::asio::local::stream_protocol::socket&& socket)
        : m_socket(std::move(socket))
    {
    }


    void LocalStream::asyncReadAtLeast(std::size_t bytesToRead, ReadCompletionCb readCompletionCb)
    {
        boost::asio::async_read(m_socket, m_buffer, boost::asio::transfer_at_least(bytesToRead), readCompletionCb);
    }

    size_t LocalStream::readAtLeast(std::size_t bytesToRead, boost::system::error_code &ec)
    {
        return boost::asio::read(m_socket, m_buffer, boost::asio::transfer_at_least(bytesToRead), ec);
    }

    void LocalStream::asyncWrite(const boost::asio::const_buffer& data, WriteCompletionCb writeCompletionCb)
    {
        boost::asio::async_write(m_socket, data, writeCompletionCb);
    }

    void LocalStream::asyncWrite(const ConstBufferVector& data, WriteCompletionCb writeCompletionCb)
    {
        boost::asio::async_write(m_socket, data, writeCompletionCb);
    }

    size_t LocalStream::write(const boost::asio::const_buffer &data, boost::system::error_code &ec)
    {
        return boost::asio::write(m_socket, data, ec);
    }

    size_t LocalStream::write(const ConstBufferVector &data, boost::system::error_code &ec)
    {
        return boost::asio::write(m_socket, data, ec);
    }


    void LocalStream::asyncClose(CompletionCb closeCb)
    {
        boost::system::error_code ec;
        m_socket.close(ec);
        closeCb(ec);
    }

    boost::system::error_code LocalStream::close()
    {
        boost::system::error_code ec;
        m_socket.close(ec);
        return ec;
    }
}

