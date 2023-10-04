#include <algorithm>

#include "boost/asio/read.hpp"
#include "boost/asio/write.hpp"


#include "stream/TcpServerStream.hpp"


namespace daq::stream {
    TcpServerStream::TcpServerStream(boost::asio::ip::tcp::socket&& socket)
        : TcpStream(std::move(socket))
    {
    }
    
    std::string TcpServerStream::endPointUrl() const
    {
        std::string streamId = m_socket.remote_endpoint().address().to_string();
        std::replace( streamId.begin(), streamId.end(), '.', '_');
        streamId += std::string("_") + std::to_string(m_socket.remote_endpoint().port());
        return streamId;
    }
    
    std::string TcpServerStream::remoteHost() const
    {
        std::string remoteHost = m_socket.remote_endpoint().address().to_string();
        return remoteHost;
    }
    
    void TcpServerStream::asyncInit(CompletionCb completionCb)
    {
        boost::system::error_code ec = init();
        completionCb(ec);
    }

    boost::system::error_code TcpServerStream::init()
    {
        return boost::system::error_code();
    }
}
