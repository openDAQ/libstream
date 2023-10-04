#include <functional>
#include <fstream>
#include <sstream>
#include <thread>
#include <boost/asio/io_context.hpp>

#include <gtest/gtest.h>

#include "stream/Stream.hpp"
#include "stream/FileStream.hpp"


namespace daq::stream {
    TEST(FileStreamTest, read)
    {
        /// Write to an existing empty file.
        std::string fileName = "theFile";
        std::string expectedContent = "there is something...";
        {
            std::ofstream file;
            file.open(fileName, std::ios_base::trunc);
            file << expectedContent;
            file.close();
        }

        boost::asio::io_context m_ioContext;

        FileStream fileStream(m_ioContext, fileName, false);

        ASSERT_EQ(fileStream.endPointUrl(), fileName);
        ASSERT_EQ(fileStream.remoteHost(), "");


        boost::system::error_code ec;
        ec = fileStream.init();
        ASSERT_EQ(ec, boost::system::error_code());
        ec = fileStream.init();
        ASSERT_EQ(ec, boost::system::error_code());

        std::size_t bytesRead = fileStream.readSome(ec);
        ASSERT_EQ(ec, boost::system::error_code());
        ASSERT_EQ(bytesRead, expectedContent.size());
        std::string result(reinterpret_cast < const char* >(fileStream.data()), fileStream.size());
        ASSERT_EQ(result, expectedContent);

        boost::system::error_code localEc;
        size_t bytesWritten = fileStream.write(boost::asio::const_buffer(expectedContent.c_str(), expectedContent.size()), localEc);
        ASSERT_EQ(bytesWritten, 0);


        m_ioContext.run();
        std::remove(fileName.c_str());
    }

    TEST(FileStreamTest, async_read)
    {
        /// Write to an existing empty file.
        std::string fileName = "theFile";
        std::string expectedContent = "there is something...";
        {
            std::ofstream file;
            file.open(fileName, std::ios_base::trunc);
            file << expectedContent;
            file.close();
        }

        boost::asio::io_context m_ioContext;

        FileStream fileStream(m_ioContext, fileName, false);

        ASSERT_EQ(fileStream.endPointUrl(), fileName);
        ASSERT_EQ(fileStream.remoteHost(), "");


        auto anotherReadCoompletionCb = [&](const boost::system::error_code& ec, std::size_t bytesRead)
        {
            if (!ec) {
                FAIL() << "unexpected read success there should be eof instead!";
            }
        };


        auto readCoompletionCb = [&](const boost::system::error_code& ec, std::size_t bytesRead)
        {
            if (ec) {
                FAIL() << "unexpected read failure: " << ec.message();
            }
            ASSERT_EQ(ec, boost::system::error_code());
            ASSERT_EQ(bytesRead, expectedContent.size());
            std::string result(reinterpret_cast < const char* >(fileStream.data()), bytesRead);
            ASSERT_EQ(result, expectedContent);
            fileStream.consume(bytesRead);
            ASSERT_EQ(fileStream.size(), 0);


            // file is read only writing has to fail!
            boost::system::error_code localEc;
            size_t bytesWritten = fileStream.write(boost::asio::const_buffer(expectedContent.c_str(), expectedContent.size()), localEc);
            ASSERT_EQ(bytesWritten, 0);

            // we have read it all next read has to fail!
            fileStream.asyncReadSome(anotherReadCoompletionCb);
        };

        auto initCompletionCb2 = [&](const boost::system::error_code& ec)
        {
            ASSERT_EQ(ec, boost::system::error_code());
            fileStream.asyncReadSome(readCoompletionCb);
        };

        auto initCompletionCb = [&](const boost::system::error_code& ec)
        {
            ASSERT_EQ(ec, boost::system::error_code());
            // try another init...
            fileStream.asyncInit(initCompletionCb2);
        };

        fileStream.asyncInit(initCompletionCb);

        m_ioContext.run();        

        std::remove(fileName.c_str());
    }

    TEST(FileStreamTest, empty_file)
    {
        std::string fileName = "theFile";

        std::string expectedContent = "there is something...";
        {
            std::ofstream file;
            file.open(fileName, std::ios_base::trunc);
            file.close();
        }

        boost::asio::io_context m_ioContext;

        FileStream fileStream(m_ioContext, fileName);

        ASSERT_EQ(fileStream.endPointUrl(), fileName);
        ASSERT_EQ(fileStream.remoteHost(), "");



        auto readCoompletionCb = [&](const boost::system::error_code& ec, std::size_t bytesRead)
        {
            ASSERT_EQ(bytesRead, 0);
            ASSERT_EQ(ec, boost::asio::error::eof);
            std::string result(reinterpret_cast < const char* >(fileStream.data()), fileStream.size());
            ASSERT_EQ(result, "");
        };


        auto initCompletionCb = [&](const boost::system::error_code& ec)
        {
            ASSERT_EQ(ec, boost::system::error_code());
            fileStream.asyncReadSome(readCoompletionCb);
        };

        fileStream.asyncInit(initCompletionCb);

        m_ioContext.run();
        std::remove(fileName.c_str());
    }

    TEST(FileStreamTest, async_write)
    {
        /// Write to an existing empty file.
        std::string fileName = "theFile";
        std::string expectedContent = "there is something written by me...";

        {
            // create an emnpty file to write to...
            std::ofstream file;
            file.open(fileName, std::ios_base::trunc);
            file.close();
        }

        boost::asio::io_context m_ioContext;
        std::thread m_ioWorker;

        FileStream fileStream(m_ioContext, fileName, true);


        auto closeCompletionCb = [&](const boost::system::error_code& ec)
        {
            ASSERT_EQ(ec, boost::system::error_code());
        };

        auto writeCompletionCb = [&](const boost::system::error_code& ec, std::size_t bytesWritten)
        {
            ASSERT_EQ(ec, boost::system::error_code());
            fileStream.asyncClose(closeCompletionCb);
        };


        auto initCompletionCb = [&](const boost::system::error_code& ec)
        {
            ASSERT_EQ(ec, boost::system::error_code());
            fileStream.asyncWrite(boost::asio::const_buffer(expectedContent.c_str(), expectedContent.size()), writeCompletionCb);
        };

        fileStream.asyncInit(initCompletionCb);

        m_ioContext.run();

        {
            // open the file and check content
            std::ifstream file;
            file.open(fileName);
            std::string fileContent;
            file >> fileContent;
            file.close();
        }
        std::remove(fileName.c_str());
    }

    TEST(FileStreamTest, async_write_parts)
    {
        /// Write to an existing empty file.
        std::string fileName = "theFile";
        std::string expectedContent = "there is something written by me...";

        {
            // create an emnpty file to write to...
            std::ofstream file;
            file.open(fileName, std::ios_base::trunc);
            file.close();
        }

        boost::asio::io_context m_ioContext;
        std::thread m_ioWorker;

        FileStream fileStream(m_ioContext, fileName, true);


        auto closeCompletionCb = [&](const boost::system::error_code& ec)
        {
            ASSERT_EQ(ec, boost::system::error_code());
        };

        auto writeCompletionCb = [&](const boost::system::error_code& ec, std::size_t bytesWritten)
        {
            ASSERT_EQ(ec, boost::system::error_code());
            fileStream.asyncClose(closeCompletionCb);
        };


        auto initCompletionCb = [&](const boost::system::error_code& ec)
        {
            ASSERT_EQ(ec, boost::system::error_code());

            ConstBufferVector buffers;
            size_t halfSize = expectedContent.size()/2;
            buffers.push_back(boost::asio::const_buffer(expectedContent.c_str(), halfSize));
            buffers.push_back(boost::asio::const_buffer(expectedContent.c_str()+halfSize, halfSize));
            fileStream.asyncWrite(buffers, writeCompletionCb);
        };

        fileStream.asyncInit(initCompletionCb);

        m_ioContext.run();

        {
            // open the file and check content
            std::ifstream file;
            file.open(fileName);
            std::string fileContent;
            file >> fileContent;
            file.close();
        }
        std::remove(fileName.c_str());
    }



    TEST(FileStreamTest, write)
    {
        /// Write to an existing empty file.
        std::string fileName = "theFile";
        std::string expectedContent = "there is something written by me...";

        {
            // create an empty file it is to be replaced!
            std::ofstream file;
            file.open(fileName, std::ios_base::trunc);
            file.close();
        }

        boost::asio::io_context m_ioContext;
        std::thread m_ioWorker;

        FileStream fileStream(m_ioContext, fileName, true);

        boost::system::error_code ec;
        ec = fileStream.init();
        ASSERT_EQ(ec, boost::system::error_code());
        fileStream.write(boost::asio::const_buffer(expectedContent.c_str(), expectedContent.size()), ec);
        ASSERT_EQ(ec, boost::system::error_code());
        ASSERT_EQ(fileStream.close(), boost::system::error_code());


        {
            // open the file and check content
            std::ifstream file;
            file.open(fileName);
            std::string content;
            std::getline (file, content);
            ASSERT_EQ(expectedContent, content);
            file.close();
        }
        std::remove(fileName.c_str());
    }

    TEST(FileStreamTest, write_parts)
    {
        /// Write to an existing empty file.
        std::string fileName = "theFile";
        std::string expectedContent = "there is something written by me...";


        // make sure to remove existing file first
        std::remove(fileName.c_str());

        boost::asio::io_context m_ioContext;
        std::thread m_ioWorker;

        FileStream fileStream(m_ioContext, fileName, true);


        auto closeCompletionCb = [&](const boost::system::error_code& ec)
        {
            ASSERT_EQ(ec, boost::system::error_code());
        };

        auto initCompletionCb = [&](const boost::system::error_code& ec)
        {
            ASSERT_EQ(ec, boost::system::error_code());
            boost::system::error_code localEc;

            ConstBufferVector buffers;
            size_t halfSize = expectedContent.size()/2;
            buffers.push_back(boost::asio::const_buffer(expectedContent.c_str(), halfSize));
            buffers.push_back(boost::asio::const_buffer(expectedContent.c_str()+halfSize, halfSize));
            fileStream.write(buffers, localEc);
            ASSERT_EQ(localEc, boost::system::error_code());
            fileStream.asyncClose(closeCompletionCb);
        };

        fileStream.asyncInit(initCompletionCb);

        m_ioContext.run();


        {
            // open the file and check content
            std::ifstream file;
            file.open(fileName);
            std::string fileContent;
            file >> fileContent;
            file.close();
        }
        std::remove(fileName.c_str());
    }


    TEST(FileStreamTest, init_failure)
    {
        /// Try operating on a non-existing file
        /// Read from a none-existing file.
        std::string fileName = "noFile";

        boost::asio::io_context m_ioContext;
        std::thread m_ioWorker;

        FileStream fileStream(m_ioContext, fileName);


        auto initCompletionCb = [&](const boost::system::error_code& ec)
        {
            ASSERT_EQ(ec, boost::system::errc::no_such_file_or_directory);
        };

        fileStream.asyncInit(initCompletionCb);

        m_ioContext.run();
    }

}
