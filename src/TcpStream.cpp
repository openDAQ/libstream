#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#ifdef _WIN32
//#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSTcpIP.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif


#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include "stream/TcpStream.hpp"

namespace daq::stream {

    TcpStream::TcpStream(boost::asio::io_context& ioc)
        : m_socket(ioc)
    {
    }

    TcpStream::TcpStream(boost::asio::ip::tcp::socket&& socket)
        : m_socket(std::move(socket))
    {
    }

    void TcpStream::asyncReadAtLeast(std::size_t bytesToRead, ReadCompletionCb readCompletionCb)
    {
        boost::asio::async_read(m_socket, m_buffer, boost::asio::transfer_at_least(bytesToRead), readCompletionCb);
    }

    size_t TcpStream::readAtLeast(std::size_t bytesToRead, boost::system::error_code &ec)
    {
        return boost::asio::read(m_socket, m_buffer, boost::asio::transfer_at_least(bytesToRead), ec);
    }

    void TcpStream::asyncWrite(const boost::asio::const_buffer& data, Stream::WriteCompletionCb writeCompletionCb)
    {
        boost::asio::async_write(m_socket, data, writeCompletionCb);
    }

    void TcpStream::asyncWrite(const ConstBufferVector& data, WriteCompletionCb writeCompletionCb)
    {
        boost::asio::async_write(m_socket, data, writeCompletionCb);
    }

    size_t TcpStream::write(const boost::asio::const_buffer &data, boost::system::error_code &ec)
    {
        return boost::asio::write(m_socket, data, ec);
    }

    size_t TcpStream::write(const ConstBufferVector &data, boost::system::error_code &ec)
    {
        return boost::asio::write(m_socket, data, ec);
    }

    void TcpStream::asyncClose(CompletionCb closeCb)
    {
        boost::system::error_code ec;
        m_socket.close(ec);
        closeCb(ec);
    }

    boost::system::error_code TcpStream::close()
    {
        boost::system::error_code ec;
        m_socket.close(ec);
        return ec;
    }

}

