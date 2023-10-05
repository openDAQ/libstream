#include <functional>
#include <future>
#include <thread>
#include <boost/asio/io_context.hpp>

#include <gtest/gtest.h>

#include "stream/Stream.hpp"
#include "stream/TcpClientStream.hpp"

#include "stream/TcpServer.hpp"

namespace daq::stream {
    class TcpStreamTest : public ::testing::Test {

    protected:
        static const std::string GoodByeMsg;
        static const uint16_t ListeningPort = 5000;

        TcpStreamTest()
            : m_server(m_ioContext, std::bind(&TcpStreamTest::NewStreamCb, this, std::placeholders::_1), ListeningPort)
        {
        }

        virtual ~TcpStreamTest()
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
            ASSERT_GT(clientHost.size(), 0);
            std::string clientUrl = m_ServerStream->endPointUrl();
            ASSERT_GT(clientUrl.size(), 0);
            asyncReadSome();
        }

        void asyncReadSome()
        {
            m_ServerStream->consume(m_ServerStream->size()); // this does nothing on the first call and consumes after each asyncWriteByte...
            m_ServerStream->asyncReadSome(std::bind(&TcpStreamTest::asyncWrite, this, std::placeholders::_1, std::placeholders::_2));
        }

        void asyncWrite(const boost::system::error_code& ec, std::size_t bytes)
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


                m_ServerStream->asyncWrite(buffers, std::bind(&TcpStreamTest::goodbyCb, this));
                return;
            }
            m_ServerStream->asyncWrite(boost::asio::buffer(m_ServerStream->data(), m_ServerStream->size()), std::bind(&TcpStreamTest::asyncReadSome, this));
        }

        void goodbyCb()
        {
            m_ServerStream->asyncClose(std::bind(&TcpStreamTest::closeCb, this, std::placeholders::_1));
        }

        void closeCb(const boost::system::error_code& ec)
        {
            m_ServerStream.reset();
        }

        boost::asio::io_context m_ioContext;
        uint8_t m_buffer[1]; // a bit silly but in the first run we process byte by byte...
        std::thread m_ioWorker;
        TcpServer m_server;
        StreamSharedPtr m_ServerStream;
    };

    const std::string TcpStreamTest::GoodByeMsg = "goodbye!";

    /// Repeatedly Create new instance, start and stop, destroy instance
    TEST(TcpServer, test_echo)
    {
        static const uint16_t ListeningPort = 5003;

        StreamSharedPtr workerStream;

        boost::asio::io_context ioContext;

        static const std::string message = "bla";

        auto closeWorkerCompleteCb = [&](const boost::system::error_code& ec)
        {
            // finished! Io context should end after leaving...
            workerStream.reset();
        };

        auto writeWorkerCompleteCb = [&](const boost::system::error_code& ec, std::size_t bytesWritten)
        {
            if (ec) {
                workerStream.reset();
                return;
            }
            workerStream->consume(bytesWritten);
            workerStream->asyncClose(closeWorkerCompleteCb);
        };


        auto readWorkerCompleteCb = [&](const boost::system::error_code& ec, std::size_t bytesRead)
        {
            if (ec) {
                workerStream.reset();
                return;
            }
            workerStream->asyncWrite(boost::asio::buffer(workerStream->data(), workerStream->size()), writeWorkerCompleteCb);
        };

        auto initWorkerCompleteCb = [&](const boost::system::error_code& ec)
        {
            if (ec) {
                workerStream.reset();
                return;
            }
            workerStream->asyncReadSome(readWorkerCompleteCb);
        };

        auto newWorkerCb = [&](StreamSharedPtr newStream)
        {
            workerStream = newStream;
            workerStream->asyncInit(initWorkerCompleteCb);
        };

        std::string response;
        TcpServer server(ioContext, newWorkerCb, ListeningPort);
        ASSERT_EQ(server.start(), 0);

        TcpClientStream client(ioContext, "localhost", std::to_string(ListeningPort));

        auto closeClientCompleteCb = [&](const boost::system::error_code& ec)
        {
            server.stop();
        };

        auto readClientCompleteCb = [&](const boost::system::error_code& ec)
        {
            if (ec) {
                FAIL() << "client read failed:" << ec.message() << std::endl;
                return;
            }
            size_t size = client.size();
            const char* data = reinterpret_cast<const char*>(client.data());
            response = std::string(data, size);
            client.consume(size);
            client.asyncClose(closeClientCompleteCb);
        };

        auto writeClientCompleteCb = [&](const boost::system::error_code& ec, std::size_t bytesWritten)
        {
            if (ec) {
                FAIL() << "client write failed:" << ec.message() << std::endl;
                return;
            }
            client.asyncRead(readClientCompleteCb, bytesWritten);
        };

        auto initClientCompleteCb = [&](const boost::system::error_code& ec)
        {
            if (ec) {
                FAIL() << "client init failed:" << ec.message() << std::endl;
                return;
            }
            client.asyncWrite(boost::asio::buffer(message), writeClientCompleteCb);
        };
        client.asyncInit(initClientCompleteCb);

        ioContext.run();
        ASSERT_EQ(message, response);
    }


#ifndef _WIN32
    TEST_F(TcpStreamTest, test_forbidden_port)
    {
        boost::asio::io_context ioc;

        auto newStreamCb = [](StreamSharedPtr newStream)
        {

        };

        ASSERT_THROW(TcpServer server(ioc, newStreamCb, 11), std::runtime_error);
    }
#endif

    TEST_F(TcpStreamTest, test_async_connect)
    {
    static const std::string hostname = "localhost";
    std::string serverHost;
    std::string endpointUrl;

        // succesfull connect
        TcpClientStream clientStream(m_ioContext, hostname, std::to_string(ListeningPort));
        serverHost = clientStream.remoteHost();
        endpointUrl = clientStream.endPointUrl();
        ASSERT_EQ(hostname, serverHost);
        ASSERT_EQ(endpointUrl, serverHost + ":" + std::to_string(ListeningPort));

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
            // init once more should be ok
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
            std::promise < boost::system::error_code > closePromise;
            std::future < boost::system::error_code > closeFuture = closePromise.get_future();

            auto completionCb = [&](const boost::system::error_code& ec)
            {
                closePromise.set_value(ec);
            };

            clientStream.asyncClose(completionCb);
            closeFuture.wait();
            ASSERT_EQ(closeFuture.get(), boost::system::error_code());
        }

        {
            std::promise < boost::system::error_code > closePromise;
            std::future < boost::system::error_code > closeFuture = closePromise.get_future();

            auto completionCb = [&](const boost::system::error_code& ec)
            {
                closePromise.set_value(ec);
            };

            clientStream.asyncClose(completionCb);
            closeFuture.wait();
            ASSERT_EQ(closeFuture.get(), boost::system::error_code());
        }
    }

    TEST_F(TcpStreamTest, test_connect)
    {
        static const std::string hostname = "localhost";
        std::string serverHost;
        std::string endpointUrl;

        // succesfull connect
        TcpClientStream clientStream(m_ioContext, hostname, std::to_string(ListeningPort));
        serverHost = clientStream.remoteHost();
        endpointUrl = clientStream.endPointUrl();
        ASSERT_EQ(hostname, serverHost);
        ASSERT_EQ(endpointUrl, serverHost + ":" + std::to_string(ListeningPort));

        boost::system::error_code ec = clientStream.init();
        ASSERT_EQ(ec, boost::system::error_code());
        ec = clientStream.init();
        ASSERT_EQ(ec, boost::system::error_code());
    }

    TEST_F(TcpStreamTest, test_connect_wrong_port)
    {
        static const std::string hostname = "localhost";

        TcpClientStream clientStream(m_ioContext, hostname, std::to_string(ListeningPort + 1));
        boost::system::error_code ec = clientStream.init();
        ASSERT_EQ(ec, boost::system::errc::connection_refused);
    }

    TEST_F(TcpStreamTest, test_connect_async_wrong_port)
    {
        static const std::string hostname = "localhost";

        TcpClientStream clientStream(m_ioContext, hostname, std::to_string(ListeningPort + 1));
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

    TEST_F(TcpStreamTest, test_connect_unknown_hostname)
    {
        static const std::string hostname = "unknown_hostname";

        TcpClientStream clientStream(m_ioContext, hostname, std::to_string(ListeningPort));

        boost::system::error_code ec = clientStream.init();
        ASSERT_NE(ec, boost::system::errc::success);
    }

    TEST_F(TcpStreamTest, test_connect_async_unknown_hostname)
    {
        static const std::string hostname = "unknown_hostname";

        TcpClientStream clientStream(m_ioContext, hostname, std::to_string(ListeningPort));

        std::promise < boost::system::error_code > initPromise;
        std::future < boost::system::error_code > initFuture = initPromise.get_future();

        auto completionCb = [&](const boost::system::error_code& ec)
        {
            initPromise.set_value(ec);
        };

        clientStream.asyncInit(completionCb);
        initFuture.wait();
        ASSERT_NE(initFuture.get(), boost::system::errc::success);
    }


    TEST_F(TcpStreamTest, test_connect_unreachable_address)
    {
        // An address that is hopefully not reachable. This will lead to a connection timeout
        TcpClientStream clientStream(m_ioContext, "1.0.0.0", std::to_string(ListeningPort));

        std::promise < boost::system::error_code > initPromise;
        std::future < boost::system::error_code > initFuture = initPromise.get_future();

        auto completionCb = [&](const boost::system::error_code& ec)
        {
            std::cerr << ec.message() << std::endl;
            initPromise.set_value(ec);
        };

        static const std::chrono::milliseconds connectionTimeout(1000);
        clientStream.asyncInit(completionCb, connectionTimeout);

        auto startTime = std::chrono::steady_clock::now();
        initFuture.wait();
        auto endTime = std::chrono::steady_clock::now();
        std::chrono::milliseconds waitMs(std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());
        ASSERT_NEAR(static_cast<double>(connectionTimeout.count()), static_cast<double>(waitMs.count()), 50);
        ASSERT_EQ(initFuture.get(), boost::system::errc::operation_canceled);
    }

    TEST_F(TcpStreamTest, test_asyncwrite_asyncread)
    {
        TcpClientStream clientStream(m_ioContext, "localhost", std::to_string(ListeningPort));

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
            boost::system::error_code ec;
            std::size_t bytesWritten;
            
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
            ASSERT_EQ(sendMessage, result);
            clientStream.consume(sendMessage.size());
        }


        // a gathered write being echoed
        std::string sendMessage2 = " world";
        {
            boost::system::error_code ec;
            std::size_t bytesWritten;
            
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
    
    
    TEST_F(TcpStreamTest, test_write_read)
    {
        boost::system::error_code ec;
        std::size_t bytesWritten;
        TcpClientStream clientStream(m_ioContext, "localhost", std::to_string(ListeningPort));

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
        bytesWritten = clientStream.write(boost::asio::buffer(sendMessage), ec);
        ASSERT_EQ(ec, boost::system::error_code());
        ASSERT_EQ(bytesWritten, sendMessage.size());

        // read in two parts
        size_t size1stPart = sendMessage.size()/2;
        size_t size2ndPart = sendMessage.size() - size1stPart;
        ec = clientStream.read(size1stPart);
        std::string result(reinterpret_cast < const char* >(clientStream.data()), size1stPart);
        clientStream.consume(size1stPart);
        ec = clientStream.read(size2ndPart);
        result += std::string(reinterpret_cast < const char* >(clientStream.data()), size2ndPart);
        clientStream.consume(size2ndPart);
        ASSERT_EQ(sendMessage, result);


        // a gathered write being echoed
        std::string sendMessage2 = " world";
        ConstBufferVector buffers;
        buffers.push_back(boost::asio::const_buffer(sendMessage.c_str(), sendMessage.size()));
        buffers.push_back(boost::asio::const_buffer(sendMessage2.c_str(), sendMessage2.size()));

        bytesWritten = clientStream.write(buffers, ec);
        ASSERT_EQ(ec, boost::system::error_code());
        ASSERT_EQ(bytesWritten, sendMessage.size()+ sendMessage2.size());

        ec = clientStream.read(sendMessage.size() + sendMessage2.size());
        result = std::string(reinterpret_cast < const char* >(clientStream.data()), clientStream.size());
        clientStream.consume(clientStream.size());
        ASSERT_EQ(sendMessage + sendMessage2, result);

        ec = clientStream.close();
        ASSERT_EQ(ec, boost::system::error_code());
    }

    TEST_F(TcpStreamTest, test_disconnect_by_server)
    {
        TcpClientStream clientStream(m_ioContext, "localhost", std::to_string(ListeningPort));

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
