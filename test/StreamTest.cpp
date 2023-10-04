#include <boost/asio/io_context.hpp>

#include <gtest/gtest.h>

#include "stream/Stream.hpp"


namespace daq::stream {
    void writeCompletionCb(const boost::system::error_code& ec, std::size_t bytesWritten)
    {
    }

    class TestStream : public Stream
    {
    public:
        TestStream()
        {
        }

        /// Start the asynchronous operation
        /// @param CompletionCb Executed after completion to start receiving/sending data
        void asyncInit(CompletionCb readCb) override
        {
        }

        boost::system::error_code init() override
        {
            return boost::system::error_code();
        }

        std::string endPointUrl() const override
        {
            return "";
        }
        std::string remoteHost() const override
        {
            return "";
        }

        void asyncReadAtLeast(std::size_t bytesToRead, ReadCompletionCb readAtLeastCb) override
        {
        }

        size_t readAtLeast(std::size_t bytesToRead, boost::system::error_code &ec) override
        {
            return 0;
        }

        void asyncWrite(const boost::asio::const_buffer& data, Stream::WriteCompletionCb writeCompletionCb) override
        {
            std::ostream os(&m_buffer);
            os.write(reinterpret_cast< const char*>(data.data()), data.size());
            writeCompletionCb(boost::system::error_code(), data.size());
        }

        void asyncWrite(const ConstBufferVector& data, WriteCompletionCb writeCompletionCb) override
        {
            size_t sizeSum = 0;
            std::ostream os(&m_buffer);
            for( const auto& iter : data) {
                os.write(reinterpret_cast< const char*>(iter.data()), iter.size());
                sizeSum += iter.size();
            }
            writeCompletionCb(boost::system::error_code(), sizeSum);
        }

        size_t write(const boost::asio::const_buffer& data, boost::system::error_code& ec) override
        {
            return 0;
        }

        size_t write(const ConstBufferVector& data, boost::system::error_code& ec) override
        {
            return 0;
        }

        void asyncClose(CompletionCb closeCb) override
        {
            closeCb(boost::system::error_code());
        }

        boost::system::error_code close() override
        {
            return boost::system::error_code();
        }
    };

    /// copyDataAndConsume copies and consumes data
    TEST(StreamTest, copyDataAndConsume_test)
    {
        static const std::string testData = "1234567890";
        char recvBuffer[128];

        TestStream testStream;
        testStream.asyncWrite(boost::asio::const_buffer(testData.c_str(), testData.size()), &writeCompletionCb);
        ASSERT_EQ(testStream.size(), testData.size());
        testStream.asyncWrite(boost::asio::const_buffer(testData.c_str(), testData.size()), &writeCompletionCb);
        ASSERT_EQ(testStream.size(), 2*testData.size());
        testStream.copyDataAndConsume(recvBuffer, testData.size());

        ASSERT_EQ(testStream.size(), testData.size());
        testStream.copyDataAndConsume(recvBuffer, testData.size());
        ASSERT_EQ(testStream.size(), 0);
    }

    /// work on data pointed to, consume manually
    /// This is working in-place to omit copying.
    TEST(StreamTest, rawData_test)
    {
        static const std::string testData = "1234567890";
        std::string received;
        size_t available;
        const uint8_t* pData;

        TestStream testStream;
        testStream.asyncWrite(boost::asio::const_buffer(testData.c_str(), testData.size()), &writeCompletionCb);
        ASSERT_EQ(testStream.size(), testData.size());
        testStream.asyncWrite(boost::asio::const_buffer(testData.c_str(), testData.size()), &writeCompletionCb);
        available = testStream.size();
        ASSERT_EQ(available, 2*testData.size());
        received = std::string(testData.data(), available/2);
        ASSERT_EQ(testData, received);
        testStream.consume(available/2);
        available = testStream.size();
        pData = testStream.data();
        ASSERT_EQ(memcmp(pData, testData.c_str(), available), 0);
        ASSERT_EQ(available, testData.size());
        testStream.consume(available);
        available = testStream.size();
        ASSERT_EQ(available, 0);

        // When there is not enough left to consume, we consume the rest without complaining...
        testStream.asyncWrite(boost::asio::const_buffer(testData.c_str(), testData.size()), &writeCompletionCb);
        available = testStream.size();
        testStream.consume(available*2);

    }

    TEST(SessioTest, asyncReadNextPart_test)
    {
        static const std::string testData = "1234567890";
        TestStream testStream;
        ConstBufferVector testDataBuffers = {
            boost::asio::const_buffer(testData.c_str(), testData.size()),
            boost::asio::const_buffer(testData.c_str(), testData.size())
        };
        testStream.asyncWrite(testDataBuffers, &writeCompletionCb);

        size_t completionCounter = 0;
        auto complectionCb = [&completionCounter, &testStream](const boost::system::error_code&)
        {
            ++completionCounter;
            testStream.consume(testData.size());
        };

        // We added the block twice. Try reading twice.
        testStream.asyncRead(complectionCb, testData.size());
        ASSERT_EQ(testStream.size(), testData.size());
        ASSERT_EQ(completionCounter, 1);
        testStream.asyncRead(complectionCb, testData.size());
        ASSERT_EQ(testStream.size(), 0);
        ASSERT_EQ(completionCounter, 2);
        // No data left. Completion callback may not be called.
        testStream.asyncRead(complectionCb, testData.size());
        ASSERT_EQ(testStream.size(), 0);
        ASSERT_EQ(completionCounter, 2);
    }

    TEST(SessioTest, asyncReadSome_test)
    {
        static const std::string testData = "1234567890";
        TestStream testStream;
        ConstBufferVector testDataBuffers = {
            boost::asio::const_buffer(testData.c_str(), testData.size()),
            boost::asio::const_buffer(testData.c_str(), testData.size())
        };
        testStream.asyncWrite(testDataBuffers, &writeCompletionCb);
        size_t bytesWritten = 0;
        for (auto iter: testDataBuffers) {
            bytesWritten += iter.size();
        }

        size_t bytesRead = 0;
        auto readComplectionCb = [&bytesRead, &testStream](const boost::system::error_code&, size_t bytes)
        {
            bytesRead += bytes;
            testStream.consume(bytes);
        };

        auto complectionCb = [](const boost::system::error_code&)
        {
        };

        // We added the block twice.

        // Read a specific part
        testStream.asyncRead(complectionCb, bytesWritten/4);
        bytesRead += bytesWritten/4;
        testStream.consume(bytesWritten/4);

        // read the rest
        testStream.asyncReadSome(readComplectionCb);
        ASSERT_EQ(bytesRead, bytesWritten);
    }
}
