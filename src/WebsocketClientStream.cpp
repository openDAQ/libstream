#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "stream/Defines.hpp"
#include "stream/WebsocketClientStream.hpp"
#include "stream/utils/boost_compatibility_utils.hpp"


namespace daq::stream {
const std::chrono::milliseconds WebsocketClientStream::DefaultConnectTimeout(5000);

WebsocketClientStream::WebsocketClientStream(boost::asio::io_context& ioc, const std::string &host, const std::string& port, const std::string &path)
    : m_ioc(ioc)
    , m_host(host)
    , m_port(port)
    , m_path(path)
    , m_stream(ioc)
    , m_resolver(ioc)
    , m_asyncOperationTimer(ioc)
    , m_asyncTimeout(DefaultConnectTimeout)
{
}

void WebsocketClientStream::asyncInit(CompletionCb completionCb)
{
    m_initCompletionCb = completionCb;
    if (m_stream.is_open()) {
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
                boost::beast::bind_front_handler(
                    &WebsocketClientStream::onResolve,
                    this));
}

void WebsocketClientStream::asyncInit(CompletionCb completionCb, std::chrono::milliseconds initTimeout)
{
    m_asyncTimeout = initTimeout;
    asyncInit(completionCb);
}

boost::system::error_code WebsocketClientStream::init()
{
    if (m_stream.is_open()) {
        return boost::system::error_code();
    }

    boost::system::error_code ec;
    boost::asio::ip::tcp::resolver::results_type results = m_resolver.resolve(m_host, m_port, ec);
    if (ec) {
        return ec;
    }

    boost::beast::get_lowest_layer(m_stream).connect(results, ec);
    if (ec) {
        return ec;
    }
    setOptions();

    boost_compatibility_utils::handshake(m_stream, m_host, m_path, ec);
    return ec;
}

void WebsocketClientStream::onResolve(const boost::beast::error_code& ec, boost::asio::ip::tcp::resolver::results_type results)
{
    if(ec) {
        m_initCompletionCb(ec);
        return;
    }

    m_asyncOperationTimer.expires_from_now(boost::posix_time::milliseconds(m_asyncTimeout.count()));
    m_asyncOperationTimer.async_wait(std::bind(&WebsocketClientStream::asyncTimeoutCb, this, std::placeholders::_1));

    // Make the connection on the IP address we got from the lookup
    boost::beast::get_lowest_layer(m_stream).async_connect(
                results,
                std::bind(&WebsocketClientStream::onConnect, this, std::placeholders::_1));
}

void WebsocketClientStream::onConnect(const boost::beast::error_code& ec)
{
    m_asyncOperationTimer.cancel();
    if (ec)
    {
        m_initCompletionCb(ec);
        return;
    }

    setOptions();

    // Perform the websocket handshake
    m_asyncOperationTimer.expires_from_now(boost::posix_time::milliseconds(m_asyncTimeout.count()));
    m_asyncOperationTimer.async_wait(std::bind(&WebsocketClientStream::asyncTimeoutCb, this, std::placeholders::_1));

    boost_compatibility_utils::async_handshake(m_stream, m_host, m_path, [this](const boost::beast::error_code& err)
    {
        this->onUpgrade(err);
    });
}

void WebsocketClientStream::onUpgrade(const boost::beast::error_code &ec)
{
    m_asyncOperationTimer.cancel();
    m_initCompletionCb(ec);
}


void WebsocketClientStream::asyncReadAtLeast(std::size_t bytesToRead, ReadCompletionCb readAtLeastCb)
{
    boost::asio::async_read(m_stream, m_buffer, boost::asio::transfer_at_least(bytesToRead), readAtLeastCb);
}

size_t WebsocketClientStream::readAtLeast(std::size_t bytesToRead, boost::system::error_code &ec)
{
    return boost::asio::read(m_stream, m_buffer, boost::asio::transfer_at_least(bytesToRead), ec);
}


void WebsocketClientStream::asyncTimeoutCb(const boost::system::error_code &ec)
{
    if (ec == boost::asio::error::operation_aborted) {
        return;
    }
    boost::beast::get_lowest_layer(m_stream).close();
}

void WebsocketClientStream::setOptions()
{
    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    boost::beast::get_lowest_layer(m_stream).expires_never();

    m_stream.binary(true);

    // Set suggested timeout settings for the websocket
    m_stream.set_option(boost::beast::websocket::stream_base::timeout::suggested(
            boost::beast::role_type::client));

    // Set a decorator to change the User-Agent of the handshake
    m_stream.set_option(boost::beast::websocket::stream_base::decorator(
            [](boost::beast::websocket::request_type& req)
            {
                    req.set(boost::beast::http::field::user_agent,
                            std::string(BOOST_BEAST_VERSION_STRING) +
                            " stream-client");
            }));
}

void WebsocketClientStream::asyncWrite(const boost::asio::const_buffer& data, Stream::WriteCompletionCb writeCompletionCb)
{
    m_stream.async_write(data, writeCompletionCb);
}

void WebsocketClientStream::asyncWrite(const ConstBufferVector& data, Stream::WriteCompletionCb writeCompletionCb)
{
    m_stream.async_write(data, writeCompletionCb);
}

size_t WebsocketClientStream::write(const boost::asio::const_buffer &data, boost::system::error_code &ec)
{
    return m_stream.write(data, ec);
}

size_t WebsocketClientStream::write(const ConstBufferVector &data, boost::system::error_code &ec)
{
    return m_stream.write(data, ec);
}

/// Before closing, handshake timeout is reduced. Otherwise timeout on dead connection would be the long 30s default!
static boost::beast::websocket::stream_base::timeout reducedHandshakeTimeout()
{
    auto options = boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client);
    options.handshake_timeout = WEBSOCKET_CLOSE_HANDSHAKE_TIMEOUT;
    return options;
}

void WebsocketClientStream::asyncClose(CompletionCb closeCb)
{
    m_stream.set_option(reducedHandshakeTimeout());
    m_stream.async_close(boost::beast::websocket::close_code::none, closeCb);
}

boost::system::error_code WebsocketClientStream::close()
{
    boost::system::error_code ec;
    m_stream.set_option(reducedHandshakeTimeout());
    m_stream.close(boost::beast::websocket::close_code::none, ec);
    return ec;
}

std::string WebsocketClientStream::endPointUrl() const
{
    return m_host + ":" + m_port + m_path;
}

std::string WebsocketClientStream::remoteHost() const
{
    return m_host;
}
}
