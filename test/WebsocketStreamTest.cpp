#include <functional>
#include <future>
#include <thread>
#include <boost/asio/io_context.hpp>

#include <gtest/gtest.h>

#include "stream/Stream.hpp"
#include "stream/WebsocketClientStream.hpp"

#include "stream/WebsocketServer.hpp"


namespace daq::stream {
    class WebsocketStreamTest : public ::testing::Test {

    protected:
        static const std::string GoodByeMsg;
        static const uint16_t ListeningPort = 5002;
        static const std::string Path;


        WebsocketStreamTest()
            : m_server(m_ioContext, std::bind(&WebsocketStreamTest::NewStreamCb, this, std::placeholders::_1), ListeningPort)
        {
        }

        virtual ~WebsocketStreamTest()
        {
        }

        virtual void SetUp()
        {
            m_server.start();
            auto threadFunction = [&]()
            {
                m_ioContext.run();
            };
            m_ioWorker = std::thread(threadFunction);
        }

        virtual void TearDown()
        {
            m_server.stop();

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
            m_ServerStream->asyncReadSome(std::bind(&WebsocketStreamTest::asyncWrite, this, std::placeholders::_1, std::placeholders::_2));
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


                m_ServerStream->asyncWrite(buffers, std::bind(&WebsocketStreamTest::goodbyCb, this));
                return;
            }
            m_ServerStream->asyncWrite(boost::asio::buffer(m_ServerStream->data(), m_ServerStream->size()), std::bind(&WebsocketStreamTest::asyncReadSome, this));
        }

        void goodbyCb()
        {
            m_ServerStream->asyncClose(std::bind(&WebsocketStreamTest::closeCb, this, std::placeholders::_1));
        }

        void closeCb(const boost::system::error_code&)
        {
            m_ServerStream.reset();
        }

        boost::asio::io_context m_ioContext;
        uint8_t m_buffer[1]; // a bit silly but in the first run we process byte by byte...
        std::thread m_ioWorker;
        WebsocketServer m_server;
        StreamSharedPtr m_ServerStream;
    };

    const std::string WebsocketStreamTest::GoodByeMsg = "goodbye!";
    /// An empty string since there is no HTTP proxy that could relay between different services
    const std::string WebsocketStreamTest::Path = "/";

    /// Repeatedly start and stop the same instance
    TEST(WebsocketServer, test_restart_stop)
    {
        static const unsigned int CYCLECOUNT = 100;
        static const uint16_t ListeningPort = 5003;
        boost::asio::io_context ioContext;

        auto newStreamCb = [](StreamSharedPtr newStream)
        {
            // will simply drop the accepted connection
        };

        WebsocketServer server(ioContext, newStreamCb, ListeningPort);
        auto threadFunction = [&]() {
            ioContext.run();
        };

        for(unsigned int count=0; count<CYCLECOUNT; ++count) {
            ASSERT_EQ(server.start(), 0);
            std::thread serverThread = std::thread(threadFunction);
            server.stop();
            serverThread.join();
        }
    }

    /// Repeatedly Create new instance, start and stop, destroy instance
    TEST(WebsocketServer, test_start_stop)
    {
        static const unsigned int CYCLECOUNT = 100;
        static const uint16_t ListeningPort = 5003;
        boost::asio::io_context ioContext;

        auto newStreamCb = [&](StreamSharedPtr newStream)
        {
            // will simply drop the accepted connection
        };

        for(unsigned int count=0; count<CYCLECOUNT; ++count) {
            WebsocketServer server(ioContext, newStreamCb, ListeningPort);
            auto threadFunction = [&]() {
                ioContext.run();
            };
            ASSERT_EQ(server.start(), 0);
            std::thread serverThread = std::thread(threadFunction);
            server.stop();
            serverThread.join();
        }
    }

    /// Repeatedly Create new instance, start and stop, destroy instance
    TEST(WebsocketServer, test_echo)
    {
        static const uint16_t ListeningPort = 5003;

        StreamSharedPtr workerStream;

        boost::asio::io_context ioContext;

        static const std::string message = "bla";
        std::string response;

        auto closeWorkerCompleteCb = [&](const boost::system::error_code& ec)
        {
            // finished! Io context should end after leaving...
            workerStream.reset();
        };

        auto writeWorkerCompleteCb = [&](const boost::system::error_code& ec, std::size_t bytesWritten)
        {
            if (ec) {
                return;
            }
            workerStream->consume(bytesWritten);
            workerStream->asyncClose(closeWorkerCompleteCb);
        };

        auto readWorkerCompleteCb = [&](const boost::system::error_code& ec, std::size_t bytesRead)
        {
            if (ec) {
                return;
            }
            workerStream->asyncWrite(boost::asio::buffer(workerStream->data(), workerStream->size()), writeWorkerCompleteCb);
        };

        auto initWorkerCompleteCb = [&](const boost::system::error_code& ec)
        {
            if (ec) {
                return;
            }
            workerStream->asyncReadSome(readWorkerCompleteCb);
        };

        auto newWorkerCb = [&](StreamSharedPtr newStream)
        {
            workerStream = newStream;
            workerStream->asyncInit(initWorkerCompleteCb);
        };

        WebsocketServer server(ioContext, newWorkerCb, ListeningPort);
        ASSERT_EQ(server.start(), 0);

        WebsocketClientStream client(ioContext, "localhost", std::to_string(ListeningPort), "/");

        auto closeClientCompleteCb = [&](const boost::system::error_code& ec)
        {
            server.stop();
        };

        auto readClientCompleteCb = [&](const boost::system::error_code& ec)
        {
            if (ec) {
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
                return;
            }
            client.asyncRead(readClientCompleteCb, bytesWritten);
        };

        auto initClientCompleteCb = [&](const boost::system::error_code& ec)
        {
            if (ec) {
                std::cerr << ec.message() << std::endl;
                return;
            }
            client.asyncWrite(boost::asio::buffer(message), writeClientCompleteCb);
        };
        client.asyncInit(initClientCompleteCb);

        ioContext.run();
        ASSERT_EQ(message, response);
    }

    TEST_F(WebsocketStreamTest, test_async_connect)
    {
        static const std::string hostname = "localhost";
        std::string serverHost;
        std::string endpointUrl;
        std::string messageSend = "ping!";
        std::string messagReceived;

        WebsocketClientStream clientStream(m_ioContext, hostname, std::to_string(ListeningPort), Path);
        for (unsigned int i = 0; i<1; i++)
        {
            // succesfull connect
            serverHost = clientStream.remoteHost();
            endpointUrl = clientStream.endPointUrl();
            ASSERT_EQ(hostname, serverHost);
            ASSERT_EQ(endpointUrl, serverHost + ":" + std::to_string(ListeningPort) + Path);

            {
                std::promise < boost::system::error_code > initPromise;
                std::future < boost::system::error_code > initFuture = initPromise.get_future();

                auto completionCb = [&](const boost::system::error_code& ec)
                {
                    initPromise.set_value(ec);
                };

                std::chrono::milliseconds connectTimeout(1000);
                clientStream.asyncInit(completionCb, connectTimeout);
                std::future_status fs = initFuture.wait_for(connectTimeout);
                ASSERT_EQ(fs, std::future_status::ready);
                ASSERT_EQ(initFuture.get(), boost::system::error_code());
            }

            {
                // another init is ok
                std::promise < boost::system::error_code > initPromise;
                std::future < boost::system::error_code > initFuture = initPromise.get_future();

                auto completionCb = [&](const boost::system::error_code& ec)
                {
                    initPromise.set_value(ec);
                };

                std::chrono::milliseconds connectTimeout(1000);
                clientStream.asyncInit(completionCb, connectTimeout);
                std::future_status fs = initFuture.wait_for(connectTimeout);
                ASSERT_EQ(fs, std::future_status::ready);
                ASSERT_EQ(initFuture.get(), boost::system::error_code());
            }

            boost::system::error_code ec;
            clientStream.write(boost::asio::buffer(messageSend), ec);
            ASSERT_EQ(ec, boost::system::error_code());
            ec = clientStream.read(messageSend.length());
            ASSERT_EQ(ec, boost::system::error_code());
            messagReceived = std::string(reinterpret_cast<const char*>(clientStream.data()), clientStream.size());
            ASSERT_EQ(messageSend, messagReceived);

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
    }

    TEST_F(WebsocketStreamTest, test_connect)
    {
        static const std::string hostname = "localhost";
        std::string serverHost;
        std::string endpointUrl;

        // succesfull connect
        WebsocketClientStream clientStream(m_ioContext, hostname, std::to_string(ListeningPort), Path);
        serverHost = clientStream.remoteHost();
        endpointUrl = clientStream.endPointUrl();
        ASSERT_EQ(hostname, serverHost);
        ASSERT_EQ(endpointUrl, serverHost + ":" + std::to_string(ListeningPort) + Path);

        boost::system::error_code ec = clientStream.init();
        ASSERT_EQ(ec, boost::system::error_code());
        ec = clientStream.init();
        ASSERT_EQ(ec, boost::system::error_code());
        ec = clientStream.close();
        ASSERT_EQ(ec, boost::system::error_code());
    }
    TEST_F(WebsocketStreamTest, test_connect_fails)
    {
        std::string hostname;
        std::string serverHost;
        std::string endpointUrl;

        // invalid hostname
        {
            hostname = "ba{}rt";
            WebsocketClientStream clientStream(m_ioContext, hostname, std::to_string(ListeningPort), Path);

            boost::system::error_code ec = clientStream.init();

            ASSERT_NE(ec, boost::system::error_code());
        }

        // wrong port
        {
            hostname = "localhost";
            WebsocketClientStream clientStream(m_ioContext, hostname, std::to_string(ListeningPort+10), Path);

            boost::system::error_code ec = clientStream.init();

            ASSERT_NE(ec, boost::system::error_code());
        }
    }

    TEST_F(WebsocketStreamTest, test_async_connect_fails)
    {
        std::string hostname;

        {
            // invalid hostname
            hostname = "ba{}rt";
            WebsocketClientStream clientStream(m_ioContext, hostname, std::to_string(ListeningPort), Path);
            std::promise < boost::system::error_code > initPromise;
            std::future < boost::system::error_code > initFuture = initPromise.get_future();

            auto completionCb = [&](const boost::system::error_code& ec)
            {
                initPromise.set_value(ec);
            };

            clientStream.asyncInit(completionCb);
            initFuture.wait();
            ASSERT_NE(initFuture.get(), boost::system::error_code());
        }

        {
            // wrong port
            hostname = "localhost";
            WebsocketClientStream clientStream(m_ioContext, hostname, std::to_string(ListeningPort + 1), Path);
            std::promise < boost::system::error_code > initPromise;
            std::future < boost::system::error_code > initFuture = initPromise.get_future();

            auto completionCb = [&](const boost::system::error_code& ec)
            {
                initPromise.set_value(ec);
            };

            clientStream.asyncInit(completionCb);
            initFuture.wait();
            boost::system::error_code ec = initFuture.get();
            ASSERT_EQ(ec, boost::system::errc::connection_refused);
        }

        {
            // wrong path
            WebsocketClientStream clientStream(m_ioContext, hostname, std::to_string(ListeningPort), "");
            std::promise < boost::system::error_code > initPromise;
            std::future < boost::system::error_code > initFuture = initPromise.get_future();

            auto completionCb = [&](const boost::system::error_code& ec)
            {
                initPromise.set_value(ec);
            };

            clientStream.asyncInit(completionCb, std::chrono::milliseconds(500));
            initFuture.wait();

            boost::system::error_code ec = initFuture.get();
            // different error codes depending on platform!
            ASSERT_NE(ec, boost::system::error_code());
        }

    }

    TEST_F(WebsocketStreamTest, test_connect_timeout)
    {
        // An address that is hopefully not reachable. This will lead to a connection timeout
        WebsocketClientStream clientStream(m_ioContext, "1.0.0.0", std::to_string(ListeningPort), Path);

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

        boost::system::error_code ec = initFuture.get();
        std::cout << ec.message() << std::endl;
        ASSERT_EQ(ec, boost::system::errc::operation_canceled);
    }

    TEST_F(WebsocketStreamTest, test_write_read)
    {
        WebsocketClientStream clientStream(m_ioContext, "localhost", std::to_string(ListeningPort), Path);

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

        std::string sendMessage = "hello";
        // a simple async write being echoed
        {
            std::promise < boost::system::error_code > writePromise;
            std::future < boost::system::error_code > writeFuture = writePromise.get_future();

            auto writeCompleCb = [&](const boost::system::error_code& ec, std::size_t bytesWritten)
            {
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

        // a simple write being echoed
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
        }
    
    }

    TEST_F(WebsocketStreamTest, test_gathered_write_read)
    {
        WebsocketClientStream clientStream(m_ioContext, "localhost", std::to_string(ListeningPort), Path);

        std::string sendMessage = "hello";
        std::string sendMessage2 = " world";

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

        // a gathered async write being echoed
        {
            std::promise < boost::system::error_code > writePromise;
            std::future < boost::system::error_code > writeFuture = writePromise.get_future();

            auto writeCompleCb = [&](const boost::system::error_code& ec, std::size_t bytesWritten)
            {
                writePromise.set_value(ec);
            };

            ConstBufferVector buffers;
            buffers.push_back(boost::asio::const_buffer(sendMessage.c_str(), sendMessage.size()));
            buffers.push_back(boost::asio::const_buffer(sendMessage2.c_str(), sendMessage2.size()));
            clientStream.asyncWrite(buffers, writeCompleCb);
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
            clientStream.asyncRead(readCb, sendMessage.size() + sendMessage2.size());
            readFuture.wait();
            ASSERT_EQ(readFuture.get(), boost::system::error_code());
            std::string result(reinterpret_cast < const char* >(clientStream.data()), clientStream.size());
            clientStream.consume(clientStream.size());
            ASSERT_EQ(sendMessage + sendMessage2, result);
        }

        // a gathered write being echoed
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

    TEST_F(WebsocketStreamTest, test_disconnect_by_server)
    {
        WebsocketClientStream clientStream(m_ioContext, "localhost", std::to_string(ListeningPort), Path);

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
            std::promise < boost::system::error_code > writePromise;
            std::future < boost::system::error_code > writeFuture = writePromise.get_future();


            auto writeCompleCb = [&](const boost::system::error_code& ec, std::size_t)
            {
                writePromise.set_value(ec);
            };
            clientStream.asyncWrite(boost::asio::buffer(GoodByeMsg), writeCompleCb);
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
            clientStream.asyncRead(readCb, GoodByeMsg.size());
            readFuture.wait();
            std::string response(reinterpret_cast<const char*>(clientStream.data()), clientStream.size());
            clientStream.consume(GoodByeMsg.size());
            ASSERT_EQ(response, GoodByeMsg);
        }
    }

    /// For testing synchronous write calls of the server side
    TEST_F(WebsocketStreamTest, test_server_write)
    {
        WebsocketClientStream clientStream(m_ioContext, "localhost", std::to_string(ListeningPort), Path);
        boost::system::error_code ec;
        std::chrono::milliseconds cycleTime(1);
        std::chrono::milliseconds waittime;
        std::chrono::milliseconds maxWatitime(100);
        
        ec = clientStream.init();
        ASSERT_EQ(ec, boost::system::error_code());

        // wait until server stream side is created...
        while (m_ServerStream == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            waittime += cycleTime;
            if (waittime >= maxWatitime) {
                FAIL() << "Server stream not created in time!";
            }
        }


        // server writes simple message
        {
            std::promise < boost::system::error_code > readPromise;
            std::future < boost::system::error_code > readFuture = readPromise.get_future();

            std::string simpleMsg = "hello!";
            m_ServerStream->write(boost::asio::const_buffer(simpleMsg.c_str(), simpleMsg.size()), ec);

            auto readCb = [&](const boost::system::error_code& ec)
            {
                readPromise.set_value(ec);
            };
            clientStream.asyncRead(readCb, simpleMsg.size());
            readFuture.wait();
            std::string response(reinterpret_cast<const char*>(clientStream.data()), clientStream.size());
            clientStream.consume(simpleMsg.size());
            ASSERT_EQ(response, simpleMsg);
        }


        // server writes gathered message
        {
            std::promise < boost::system::error_code > readPromise;
            std::future < boost::system::error_code > readFuture = readPromise.get_future();


            std::string msgPart1 = "hello";
            std::string msgPart2 = " world!";


            std::size_t bytesWritten;

            ConstBufferVector buffers;
            buffers.push_back(boost::asio::const_buffer(msgPart1.c_str(), msgPart1.size()));
            buffers.push_back(boost::asio::const_buffer(msgPart2.c_str(), msgPart2.size()));

            bytesWritten = clientStream.write(buffers, ec);
            ASSERT_EQ(ec, boost::system::error_code());
            ASSERT_EQ(bytesWritten, msgPart1.size() + msgPart2.size());



            auto readCb = [&](const boost::system::error_code& ec)
            {
                readPromise.set_value(ec);
            };
            clientStream.asyncRead(readCb, msgPart1.size() + msgPart2.size());
            readFuture.wait();
            std::string response(reinterpret_cast<const char*>(clientStream.data()), clientStream.size());
            clientStream.consume(msgPart1.size() + msgPart2.size());
            ASSERT_EQ(response, msgPart1 + msgPart2);
        }
    }
}
