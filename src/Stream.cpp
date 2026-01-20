#include "stream/Stream.hpp"
#include <boost/asio/buffer.hpp>

namespace daq::stream {

void Stream::copyDataAndConsume(void* dest, size_t size)
{
    auto buffers = m_buffer.data();
    // For streambuf, use boost::asio::buffer() to get a single buffer view
    // This works because streambuf::data() returns a contiguous buffer sequence
    auto buf = boost::asio::buffer(buffers);
    // In Boost.Asio 1.90+, const_buffer has a data() member function
    const void* data_ptr = buf.data();
    std::memcpy(dest, data_ptr, size);
    m_buffer.consume(size);
}

size_t Stream::size() const
{
    return m_buffer.size();
}

const uint8_t* Stream::data() const
{
    auto buffers = m_buffer.data();
    // Use boost::asio::buffer() to get a single buffer view from the sequence
    auto buf = boost::asio::buffer(buffers);
    // const_buffer has a data() member function that returns const void*
    return static_cast<const uint8_t*>(buf.data());
}

void Stream::consume(size_t size)
{
    m_buffer.consume(size);
}

void Stream::asyncRead(CompletionCb readCb, std::size_t size)
{
    size_t remainingData = m_buffer.size();
    if (remainingData >= size)
    {
        // all required data is already in the buffer
        readCb(boost::system::error_code());
    }
    else
    {
        // read at least the required amount of data into the buffer
        asyncReadAtLeast(size-remainingData, std::bind(readCb, std::placeholders::_1));
    }
}

boost::system::error_code Stream::read(std::size_t size)
{
    size_t remainingData = m_buffer.size();
    if (remainingData >= size)
    {
        // all required data is already in the buffer
        return boost::system::error_code();
    }
    else
    {
        // read at least the required amount of data into the buffer
        boost::system::error_code ec;
        readAtLeast(size-remainingData, ec);
        return ec;
    }
}

void Stream::asyncReadSome(ReadCompletionCb readCb)
{
    size_t remainingData = m_buffer.size();
    if (remainingData)
        readCb(boost::system::error_code(), remainingData);
    else
        asyncReadAtLeast(1, readCb);
}

size_t Stream::readSome(boost::system::error_code& ec)
{
    size_t remainingData = m_buffer.size();
    if (remainingData) {
        ec = boost::system::error_code();
        return remainingData;
    } else {
        return readAtLeast(1, ec);
    }
}
}
