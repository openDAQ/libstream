#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include "stream/TcpClientStream.hpp"

namespace daq::stream {

    const std::chrono::milliseconds TcpClientStream::DefaultConnectTimeout(5000);

    TcpClientStream::TcpClientStream(boost::asio::io_context& ioc, const std::string &host, const std::string &port)
        : TcpStream(ioc)
        , m_ioc(ioc)
        , m_host(host)
        , m_port(port)
        , m_resolver(ioc)
        , m_connectTimer(ioc)
        , m_connectTimeout(DefaultConnectTimeout)
    {
    }

    void TcpClientStream::asyncInit(CompletionCb completionCb)
    {
        m_initCompletionCb = completionCb;
        if (m_socket.is_open()) {
            auto completion = [this]()
            {
                m_initCompletionCb(boost::system::error_code());
            };
            m_ioc.dispatch(completion);
            return;
        }
        // Look up the domain name.
        m_resolver.async_resolve(
                    m_host,
                    m_port,
                    std::bind(&TcpClientStream::onResolve, this, std::placeholders::_1, std::placeholders::_2));
    }

    void TcpClientStream::asyncInit(CompletionCb completionCb, std::chrono::milliseconds connectTimeout)
    {
        m_connectTimeout = connectTimeout;
        asyncInit(completionCb);
    }

    boost::system::error_code TcpClientStream::init()
    {
        if (m_socket.is_open()) {
            return boost::system::error_code();
        }

        boost::system::error_code ec;
        boost::asio::ip::tcp::resolver::results_type results = m_resolver.resolve(m_host, m_port, ec);
        if (ec) {
            return ec;
        }

        // Make the connection on the IP address we got from the lookup
        m_socket.connect(results.begin()->endpoint(), ec);
        return ec;
    }

    std::string TcpClientStream::endPointUrl() const
    {
        return m_host + ":" + m_port;
    }

    std::string TcpClientStream::remoteHost() const
    {
        return m_host;
    }

    void TcpClientStream::onResolve(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results)
    {
        if(ec)
        {
            m_initCompletionCb(ec);
            return;
        }

        m_connectTimer.expires_from_now(boost::posix_time::milliseconds(m_connectTimeout.count()));
        m_connectTimer.async_wait(std::bind(&TcpClientStream::connectTimeoutCb, this, std::placeholders::_1));
        // Make the connection on the IP address we got from the lookup
        m_socket.async_connect(
                    results.begin()->endpoint(),
                    std::bind(&TcpClientStream::onConnect, this, std::placeholders::_1));
    }

    void TcpClientStream::onConnect(const boost::system::error_code &ec)
    {
        m_connectTimer.cancel();
        m_initCompletionCb(ec);
    }

    void TcpClientStream::connectTimeoutCb(const boost::system::error_code& ec)
    {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        m_socket.close();
    }
}

