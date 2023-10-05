#include <functional>
#include <future>
#include <thread>
#include <boost/asio/io_context.hpp>

#include <gtest/gtest.h>

#include "stream/Stream.hpp"
#include "stream/LocalClientStream.hpp"

#include "stream/LocalServer.hpp"
#include "stream/LocalServerStream.hpp"


namespace daq::stream {
    class LocalStreamTest : public ::testing::Test {

    protected:
        static const std::string GoodByeMsg;
        static const std::string localEndpointFile;

        LocalStreamTest()
            : m_server(m_ioContext, std::bind(&LocalStreamTest::NewStreamCb, this, std::placeholders::_1), localEndpointFile)
        {
        }

        virtual ~LocalStreamTest()
        {
        }

        virtual void SetUp()
        {
            m_server.start();
            auto threadFunction = [&]() {
                m_ioContext.run();
                std::cout << "Left io context" << std::endl;

            };
            m_ioWorker = std::thread(threadFunction);
        }

        virtual void TearDown()
        {
            m_server.stop();


            if (m_ServerStream) {
                std::promise < boost::system::error_code > closePromise;
                std::future < boost::system::error_code > closeFuture = closePromise.get_future();

                auto closeCb = [&](const boost::system::error_code& ec)
                {
                    closePromise.set_value(ec);
                };
                m_ServerStream->asyncClose(closeCb);
                closeFuture.wait();
                ASSERT_EQ(closeFuture.get(), boost::system::error_code());
            }
            m_ioContext.stop();
            m_ioWorker.join();
        }


        void NewStreamCb(StreamSharedPtr newStream)
        {
            m_ServerStream = newStream;
            std::string clientHost = m_ServerStream->remoteHost();
            // this is hard to know, could be "localhost", "127.0.0.1", "::1", or "::ffff:127.0.0.1"
            ASSERT_EQ(clientHost.size(), 0);
            std::string clientUrl = m_ServerStream->endPointUrl();
            asyncReadSome();
        }

        void asyncReadSome()
        {
            m_ServerStream->consume(m_ServerStream->size()); // this does nothing on the first call and consumes after each asyncWriteByte...
            m_ServerStream->asyncReadSome(std::bind(&LocalStreamTest::asyncWrite, this, std::placeholders::_1, std::placeholders::_2));
        }

        void asyncWrite(const boost::system::error_code& ec, std::size_t)
        {
            if (ec) {
                return;
            }
            std::string received(reinterpret_cast < const char* >(m_ServerStream->data()), m_ServerStream->size());
            if(received == GoodByeMsg)
            {
                ConstBufferVector buffers;
                buffers.push_back(boost::asio::const_buffer(GoodByeMsg.c_str(), GoodByeMsg.size()/2));
                buffers.push_back(boost::asio::const_buffer(GoodByeMsg.c_str()+GoodByeMsg.size()/2, GoodByeMsg.size()/2));


                m_ServerStream->asyncWrite(buffers, std::bind(&LocalStreamTest::goodbyCb, this));
                return;
            }
            m_ServerStream->asyncWrite(boost::asio::buffer(m_ServerStream->data(), m_ServerStream->size()), std::bind(&LocalStreamTest::asyncReadSome, this));
        }

        void goodbyCb()
        {
            m_ServerStream->asyncClose(std::bind(&LocalStreamTest::closeCb, this, std::placeholders::_1));
        }

        void closeCb(const boost::system::error_code&)
        {
            m_ServerStream.reset();
        }



        boost::asio::io_context m_ioContext;
        uint8_t m_buffer[1]; // a bit silly but in the first run we process byte by byte...
        std::thread m_ioWorker;
        LocalServer m_server;
        StreamSharedPtr m_ServerStream;
    };

    const std::string LocalStreamTest::localEndpointFile = "theEndpoint";
    const std::string LocalStreamTest::GoodByeMsg = "goodbye!";



    TEST_F(LocalStreamTest, test_connect)
    {
        std::string serverHost;
        std::string endpointUrl;

        {
            // succesfull connect
            LocalClientStream clientStream(m_ioContext, localEndpointFile);
            serverHost = clientStream.remoteHost();
            endpointUrl = clientStream.endPointUrl();
            ASSERT_EQ("", serverHost);

            boost::system::error_code ec = clientStream.init();
            ASSERT_EQ(ec, boost::system::error_code());
            ec = clientStream.init();
            ASSERT_EQ(ec, boost::system::error_code());
        }

        {
            // wrong endpoint file
            LocalClientStream clientStream(m_ioContext, localEndpointFile + "bla");

            boost::system::error_code ec = clientStream.init();
            ASSERT_EQ(ec, boost::system::errc::connection_refused);
        }
    }

    TEST_F(LocalStreamTest, test_async_connect)
    {
        std::string serverHost;
        std::string endpointUrl;

        {
            // succesfull connect
            LocalClientStream clientStream(m_ioContext, localEndpointFile);
            serverHost = clientStream.remoteHost();
            endpointUrl = clientStream.endPointUrl();
            ASSERT_EQ("", serverHost);

            std::promise < boost::system::error_code > initPromise;
            std::future < boost::system::error_code > initFuture = initPromise.get_future();

            auto completionCb = [&](const boost::system::error_code& ec)
            {
                initPromise.set_value(ec);
            };

            clientStream.asyncInit(completionCb);
            initFuture.wait();
            ASSERT_EQ(initFuture.get(), boost::system::error_code());
        }

        {
            // wrong endpoint file
            LocalClientStream clientStream(m_ioContext, localEndpointFile + "bla");

            std::promise < boost::system::error_code > initPromise;
            std::future < boost::system::error_code > initFuture = initPromise.get_future();

            auto completionCb = [&](const boost::system::error_code& ec)
            {
                initPromise.set_value(ec);
            };

            clientStream.asyncInit(completionCb);
            initFuture.wait();
            ASSERT_EQ(initFuture.get(), boost::system::errc::connection_refused);
        }
    }


    TEST_F(LocalStreamTest, test_asyncwrite_asyncread)
    {
        LocalClientStream clientStream(m_ioContext, localEndpointFile);

        {
            std::promise < boost::system::error_code > initPromise;
            std::future < boost::system::error_code > initFuture = initPromise.get_future();

            auto completionCb = [&](const boost::system::error_code& ec)
            {
                std::cout << ec << std::endl;
                initPromise.set_value(ec);
            };


            clientStream.asyncInit(completionCb);
            initFuture.wait();
            ASSERT_EQ(initFuture.get(), boost::system::error_code());
        }

        // a simple write being echoed
        std::string sendMessage = "hello";
        {
            std::promise < boost::system::error_code > writePromise;
            std::future < boost::system::error_code > writeFuture = writePromise.get_future();

            auto writeCompleCb = [&](const boost::system::error_code& ec, std::size_t bytesWritten)
            {
                std::cout << ec << " " << bytesWritten << std::endl;
                writePromise.set_value(ec);
            };
            clientStream.asyncWrite(boost::asio::buffer(sendMessage), writeCompleCb);
            writeFuture.wait();
            ASSERT_EQ(writeFuture.get(), boost::system::error_code());
        }

        {
            std::promise < boost::system::error_code > readPromise;
            std::future < boost::system::error_code > readFuture = readPromise.get_future();

            auto readCb = [&](const boost::system::error_code& ec)
            {
                readPromise.set_value(ec);
            };
            clientStream.asyncRead(readCb, sendMessage.size());
            readFuture.wait();
            ASSERT_EQ(readFuture.get(), boost::system::error_code());
            std::string result(reinterpret_cast < const char* >(clientStream.data()), clientStream.size());
            ASSERT_EQ(sendMessage, result);
        }

        // a gathered write being echoed
        std::string sendMessage2 = " world";
        {
            std::promise < boost::system::error_code > writePromise;
            std::future < boost::system::error_code > writeFuture = writePromise.get_future();

            auto writeCompleCb = [&](const boost::system::error_code& ec, std::size_t)
            {
                writePromise.set_value(ec);
            };

            ConstBufferVector buffers;
            buffers.push_back(boost::asio::const_buffer(sendMessage.c_str(), sendMessage.size()));
            buffers.push_back(boost::asio::const_buffer(sendMessage2.c_str(), sendMessage2.size()));
            clientStream.asyncWrite(buffers, writeCompleCb);
            writeFuture.wait();
            ASSERT_EQ(writeFuture.get(), boost::system::error_code());
            std::string result(reinterpret_cast < const char* >(clientStream.data()), clientStream.size());
            clientStream.consume(clientStream.size());
            ASSERT_EQ(sendMessage, result);
        }

        {
            std::promise < boost::system::error_code > readPromise;
            std::future < boost::system::error_code > readFuture = readPromise.get_future();

            auto readCb = [&](const boost::system::error_code& ec)
            {
                readPromise.set_value(ec);
            };
            clientStream.asyncRead(readCb, sendMessage.size() + sendMessage2.size());
            readFuture.wait();
            ASSERT_EQ(readFuture.get(), boost::system::error_code());
            std::string result(reinterpret_cast < const char* >(clientStream.data()), clientStream.size());
            clientStream.consume(clientStream.size());
            ASSERT_EQ(sendMessage + sendMessage2, result);
        }

        {
            std::promise < boost::system::error_code > closePromise;
            std::future < boost::system::error_code > closeFuture = closePromise.get_future();

            auto closeCb = [&](const boost::system::error_code& ec)
            {
                closePromise.set_value(ec);
            };

            clientStream.asyncClose(closeCb);
            closeFuture.wait();
            ASSERT_EQ(closeFuture.get(), boost::system::error_code());
        }
    }

    TEST_F(LocalStreamTest, test_write_read)
    {
        LocalClientStream clientStream(m_ioContext, localEndpointFile);
        boost::system::error_code ec;
        ec = clientStream.init();

        // a simple write being echoed
        std::string sendMessage = "hello";
        clientStream.write(boost::asio::buffer(sendMessage), ec);
        ec = clientStream.read(sendMessage.size());
        std::string result = std::string(reinterpret_cast < const char* >(clientStream.data()), clientStream.size());
        clientStream.consume(clientStream.size());
        ASSERT_EQ(sendMessage, result);

        // a gathered write being echoed
        std::string sendMessage2 = " world";
        ConstBufferVector buffers;
        buffers.push_back(boost::asio::const_buffer(sendMessage.c_str(), sendMessage.size()));
        buffers.push_back(boost::asio::const_buffer(sendMessage2.c_str(), sendMessage2.size()));
        clientStream.write(buffers, ec);
        size_t size = sendMessage.size() + sendMessage2.size();
        ec = clientStream.read(size);
        result = std::string(reinterpret_cast < const char* >(clientStream.data()), clientStream.size());
        clientStream.consume(size);
        ASSERT_EQ(sendMessage + sendMessage2, result);
        std::promise < boost::system::error_code > closePromise;
        std::future < boost::system::error_code > closeFuture = closePromise.get_future();

        ec = clientStream.close();
        ASSERT_EQ(ec, boost::system::error_code());
    }

    TEST_F(LocalStreamTest, test_write_async)
    {
        boost::system::error_code ec;
        std::size_t bytesWritten;
        LocalClientStream clientStream(m_ioContext, localEndpointFile);
        
        
        {
            std::promise < boost::system::error_code > initPromise;
            std::future < boost::system::error_code > initFuture = initPromise.get_future();

            auto completionCb = [&](const boost::system::error_code& ec)
            {
                std::cout << ec << std::endl;
                initPromise.set_value(ec);
            };


            clientStream.asyncInit(completionCb);
            initFuture.wait();
            ASSERT_EQ(initFuture.get(), boost::system::error_code());
        }

        // a simple write being echoed
        std::string sendMessage = "hello";
        {
            bytesWritten = clientStream.write(boost::asio::buffer(sendMessage), ec);
            ASSERT_EQ(ec, boost::system::error_code());
            ASSERT_EQ(bytesWritten, sendMessage.size());
        }

        {
            std::promise < boost::system::error_code > readPromise;
            std::future < boost::system::error_code > readFuture = readPromise.get_future();

            auto readCb = [&](const boost::system::error_code& ec)
            {
                readPromise.set_value(ec);
            };
            clientStream.asyncRead(readCb, sendMessage.size());
            readFuture.wait();
            ASSERT_EQ(readFuture.get(), boost::system::error_code());
            std::string result(reinterpret_cast < const char* >(clientStream.data()), clientStream.size());
            clientStream.consume(clientStream.size());
            ASSERT_EQ(sendMessage, result);
        }


        // a gathered write being echoed
        std::string sendMessage2 = " world";
        {
            ConstBufferVector buffers;
            buffers.push_back(boost::asio::const_buffer(sendMessage.c_str(), sendMessage.size()));
            buffers.push_back(boost::asio::const_buffer(sendMessage2.c_str(), sendMessage2.size()));
            
            bytesWritten = clientStream.write(buffers, ec);
            ASSERT_EQ(ec, boost::system::error_code());
            ASSERT_EQ(bytesWritten, sendMessage.size()+ sendMessage2.size());
        }

        {
            std::promise < boost::system::error_code > readPromise;
            std::future < boost::system::error_code > readFuture = readPromise.get_future();

            auto readCb = [&](const boost::system::error_code& ec)
            {
                readPromise.set_value(ec);
            };
            clientStream.asyncRead(readCb, sendMessage.size() + sendMessage2.size());
            readFuture.wait();
            ASSERT_EQ(readFuture.get(), boost::system::error_code());
            std::string result(reinterpret_cast < const char* >(clientStream.data()), clientStream.size());
            clientStream.consume(clientStream.size());
            ASSERT_EQ(sendMessage + sendMessage2, result);
        }

        {
            std::promise < boost::system::error_code > closePromise;
            std::future < boost::system::error_code > closeFuture = closePromise.get_future();

            auto closeCb = [&](const boost::system::error_code& ec)
            {
                closePromise.set_value(ec);
            };

            clientStream.asyncClose(closeCb);
            closeFuture.wait();
            ASSERT_EQ(closeFuture.get(), boost::system::error_code());
        }
    }
    
    TEST_F(LocalStreamTest, test_disconnect_by_server)
    {
        LocalClientStream clientStream(m_ioContext, localEndpointFile);

        {
            std::promise < boost::system::error_code > initPromise;
            std::future < boost::system::error_code > initFuture = initPromise.get_future();

            auto completionCb = [&](const boost::system::error_code& ec)
            {
                initPromise.set_value(ec);
            };


            clientStream.asyncInit(completionCb);
            initFuture.wait();
            ASSERT_EQ(initFuture.get(), boost::system::error_code());
        }

        {
            boost::system::error_code ec;
            std::size_t bytesWritten;
            
            bytesWritten = clientStream.write(boost::asio::buffer(GoodByeMsg), ec);
            ASSERT_EQ(ec, boost::system::error_code());
            ASSERT_EQ(bytesWritten, GoodByeMsg.size());
        }

        {
            std::promise < boost::system::error_code > readPromise;
            std::future < boost::system::error_code > readFuture = readPromise.get_future();

            auto readCb = [&](const boost::system::error_code& ec)
            {
                readPromise.set_value(ec);
            };
            clientStream.asyncRead(readCb, GoodByeMsg.size());
            readFuture.wait();
            std::string response(reinterpret_cast<const char*>(clientStream.data()), clientStream.size());
            clientStream.consume(GoodByeMsg.size());
            ASSERT_EQ(response, GoodByeMsg);
        }
    }
}
