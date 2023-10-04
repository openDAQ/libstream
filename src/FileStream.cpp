#include <cstdlib>
#include <string>

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include "stream/FileStream.hpp"


namespace daq::stream {
    /// \param writable If true, new file will be created. Existing one will be replaced!
    FileStream::FileStream(boost::asio::io_context& ioc, const std::string &fileName, bool writable)
        : m_ioc(ioc)
        , m_fileName(fileName)
        , m_fileStream(ioc)
        , m_writable(writable)
    {
    }

    void FileStream::asyncInit(CompletionCb initCb)
    {
        m_initCompletionCb = initCb;
        boost::system::error_code ec = init();
        // ec is on stack, provide it by value!
        auto completionCb = [this, ec]()
        {
            m_initCompletionCb(ec);
        };
        m_ioc.dispatch(completionCb);
    }

    boost::system::error_code FileStream::init()
    {
        if(m_fileStream.is_open()) {
            return boost::system::error_code();
        }
#ifdef _WIN32
        LPCSTR pFileName = m_fileName.c_str();
        DWORD desiredAccess;
        DWORD shareMode = 0;
        LPSECURITY_ATTRIBUTES securityAttributes = nullptr;
        DWORD creationDisposition;
        DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED;
        HANDLE templateFile = nullptr;


        if (m_writable) {
            desiredAccess = GENERIC_WRITE;
            creationDisposition = CREATE_ALWAYS;
        }
        else {
            desiredAccess = GENERIC_READ;
            creationDisposition = OPEN_EXISTING;
        }

        HANDLE fd = ::CreateFileA(
            pFileName,
            desiredAccess,
            shareMode,
            securityAttributes,
            creationDisposition,
            flagsAndAttributes,
            templateFile
        );

        if (fd == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            return boost::system::error_code(error, boost::system::generic_category());
        }
#else
        int flags = O_NONBLOCK;
        if (m_writable) {
            flags |= O_TRUNC | O_CREAT | O_RDWR;
        }
        int fd = ::open(m_fileName.c_str(), flags, 0777);
        if (fd == -1) {
            return boost::system::error_code(errno, boost::system::generic_category());
        }

#endif
        boost::system::error_code ec;
        m_fileStream.assign(fd, ec);
        return ec;
    }

    std::string FileStream::endPointUrl() const
    {
        return m_fileName;
    }

    std::string FileStream::remoteHost() const
    {
        return "";
    }

    void FileStream::asyncReadAtLeast(std::size_t bytesToRead, ReadCompletionCb readCompletionCb)
    {
        boost::asio::async_read(m_fileStream, m_buffer, boost::asio::transfer_at_least(bytesToRead), readCompletionCb);
    }

    size_t FileStream::readAtLeast(std::size_t bytesToRead, boost::system::error_code &ec)
    {
        return boost::asio::read(m_fileStream, m_buffer, boost::asio::transfer_at_least(bytesToRead), ec);
    }

    void FileStream::asyncWrite(const boost::asio::const_buffer& data, WriteCompletionCb writeCompletionCb)
    {
        boost::asio::async_write(m_fileStream, data, writeCompletionCb);
    }

    void FileStream::asyncWrite(const std::vector<boost::asio::const_buffer>& data, WriteCompletionCb writeCompletionCb)
    {
        boost::asio::async_write(m_fileStream, data, writeCompletionCb);
    }

    size_t FileStream::write(const boost::asio::const_buffer &data, boost::system::error_code &ec)
    {
        return boost::asio::write(m_fileStream, data, ec);
    }

    size_t FileStream::write(const ConstBufferVector &data, boost::system::error_code &ec)
    {
        return boost::asio::write(m_fileStream, data, ec);
    }

    void FileStream::asyncClose(CompletionCb closeCb)
    {
        m_fileStream.close();
        closeCb(boost::system::error_code());
    }

    boost::system::error_code FileStream::close()
    {
        m_fileStream.close();
        return boost::system::error_code();
    }
}
