#include <algorithm>
#include <iostream>

#include <boost/asio/read.hpp>
#include "boost/beast/websocket/stream.hpp"

#include "stream/Defines.hpp"
#include "stream/WebsocketServerStream.hpp"

namespace daq::stream {
    WebsocketServerStream::WebsocketServerStream(std::shared_ptr<boost::beast::websocket::stream<boost::beast::tcp_stream> > websocket)
        : m_websocket(websocket)
    {
    }
    
    void WebsocketServerStream::asyncInit(CompletionCb completionCb)
    {
        boost::system::error_code ec = init();
        completionCb(ec);
    }

    boost::system::error_code WebsocketServerStream::init()
    {
        setOptions();
        return boost::system::error_code();
    }

    /// Before closing, handshake timeout is reduced. Otherwise timeout on dead connection would be the long 30s default!
    static boost::beast::websocket::stream_base::timeout reducedHandshakeTimeout()
    {
        auto options = boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server);
        options.handshake_timeout = WEBSOCKET_CLOSE_HANDSHAKE_TIMEOUT;
        return options;
    }
    
    void WebsocketServerStream::asyncClose(CompletionCb closeCb)
    {
        m_websocket->set_option(reducedHandshakeTimeout());
        m_websocket->async_close(boost::beast::websocket::close_code::none, closeCb);
    }

    boost::system::error_code WebsocketServerStream::close()
    {
        boost::system::error_code ec;
        m_websocket->set_option(reducedHandshakeTimeout());
        m_websocket->close(boost::beast::websocket::close_code::none, ec);
        return ec;
    }
    
    std::string WebsocketServerStream::endPointUrl() const
    {
        std::string streamId = m_websocket->next_layer().socket().remote_endpoint().address().to_string();
        std::replace( streamId.begin(), streamId.end(), '.', '_');
        streamId += std::string("_") + std::to_string(m_websocket->next_layer().socket().remote_endpoint().port());
        return streamId;
    }

    std::string WebsocketServerStream::remoteHost() const
    {
        std::string remoteHost = m_websocket->next_layer().socket().remote_endpoint().address().to_string();
        return remoteHost;
    }
    
    void WebsocketServerStream::asyncReadAtLeast(std::size_t bytesToRead, ReadCompletionCb readAtLeastCb)
    {
        boost::asio::async_read(*m_websocket, m_buffer, boost::asio::transfer_at_least(bytesToRead), readAtLeastCb);
    }

    size_t WebsocketServerStream::readAtLeast(std::size_t bytesToRead, boost::system::error_code &ec)
    {
        return boost::asio::read(*m_websocket, m_buffer, boost::asio::transfer_at_least(bytesToRead), ec);
    }
    
    void WebsocketServerStream::asyncWrite(const boost::asio::const_buffer& data, WriteCompletionCb writeCompletionCb)
    {
#if defined(__GNUC__)
#pragma GCC diagnostic push
        // we want to ignore a warning coming from boost beast
#pragma GCC diagnostic warning "-Wstrict-overflow"
#endif
    	
        m_websocket->async_write(data, writeCompletionCb);

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    }
    
    void WebsocketServerStream::asyncWrite(const ConstBufferVector& data, WriteCompletionCb writeCompletionCb)
    {
#if defined(__GNUC__)
#pragma GCC diagnostic push
        // we want to ignore a warning coming from boost beast
#pragma GCC diagnostic warning "-Wstrict-overflow"
#endif
    	
        m_websocket->async_write(data, writeCompletionCb);

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    }

    size_t WebsocketServerStream::write(const boost::asio::const_buffer &data, boost::system::error_code &ec)
    {
        return m_websocket->write(data, ec);
    }

    size_t WebsocketServerStream::write(const ConstBufferVector &data, boost::system::error_code &ec)
    {
        return m_websocket->write(data, ec);
    }
    void WebsocketServerStream::setOptions()
    {
        m_websocket->binary(true);
        //m_stream->auto_fragment(true);
        m_websocket->set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        auto autodecoratorHandler = [](boost::beast::websocket::response_type& res)
        {
            res.set(boost::beast::http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                    " stream-server");
        };
        m_websocket->set_option(boost::beast::websocket::stream_base::decorator(autodecoratorHandler));
    }
}
