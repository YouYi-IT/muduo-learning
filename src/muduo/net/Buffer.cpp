#include "Buffer.h"

#include <algorithm>
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

Buffer::Buffer(size_t initial_size)
    : buffer_(kCheapPrepend + initial_size),
      reader_index_(kCheapPrepend),
      writer_index_(kCheapPrepend)
{
}

size_t Buffer::readableBytes() const
{
    return writer_index_ - reader_index_;
}
size_t Buffer::writableBytes() const
{
    return buffer_.size() - writer_index_;
}
size_t Buffer::prependableBytes() const
{
    return reader_index_;
}

// 返回缓冲区中可读数据的起始地址
const char* Buffer::peek() const
{
    return begin() + reader_index_;
}

void Buffer::retrieve(size_t len)
{
    if (len < readableBytes())
    {
        reader_index_ += len;
    }
    else
    {
        retrieveAll();
    }
}
void Buffer::retrieveAll()
{
    reader_index_ = writer_index_ = kCheapPrepend;
}

// 把onMessage函数上报的Bufferr数据，转成string类型的数据返回
std::string Buffer::retrieveAllString()
{
    return retrieveAsString(readableBytes());
}
std::string Buffer::retrieveAsString(size_t len)
{
    std::string result(peek(), len);
    retrieve(len);  // 上面一行把缓冲区中可读的数据都出来了，现在要移动可读指针位置
    return result;
}

void Buffer::ensureWritableBytes(size_t len)
{
    if (writableBytes() < len)
    {
        makeSpace(len);
    }
}

void Buffer::append(const char* data, size_t len)
{
    ensureWritableBytes(len);
    std::copy(data, data + len, beginWriter());
    writer_index_ += len;
}

ssize_t Buffer::readFd(int fd, int* save_errno)
{
    char extrabuf[65535] = {0};  // 栈上的内存空间
    struct iovec vec[2];
    const size_t writable = writableBytes();
    vec[0].iov_base = beginWriter();
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;  // 一次最多读64K
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *save_errno = errno;
    }
    else if (n <= static_cast<ssize_t>(writable))
    {
        writer_index_ += n;
    }
    else  // extrabuf里面也写入了数据
    {
        writer_index_ = buffer_.size();
        append(extrabuf, n - writable);
    }

    return n;
}

// 向fd上发送数据
ssize_t Buffer::writeFd(int fd, int* save_errno)
{
    ssize_t n=::write(fd,peek(),readableBytes());
    if(n<0)
    {
        *save_errno=errno;
    }
    return n;
}

char* Buffer::begin()
{
    // &*buffer_.begin() 等价于buffer_.data()
    // 如果 buffer_ 非空，它基本等价于：&buffer_[0]
    return buffer_.data();
}
const char* Buffer::begin() const
{
    return buffer_.data();
}

char* Buffer::beginReader()
{
    return begin() + reader_index_;
}
const char* Buffer::beginReader() const
{
    return begin() + reader_index_;
}

char* Buffer::beginWriter()
{
    return begin() + writer_index_;
}
const char* Buffer::beginWriter() const
{
    return begin() + writer_index_;
}

void Buffer::makeSpace(size_t len)
{
    if (writableBytes() + prependableBytes() < len + kCheapPrepend)
    {
        buffer_.resize(writer_index_ + len);
    }
    else
    {
        size_t readable = readableBytes();
        std::copy(beginReader(), begin() + writer_index_, begin() + kCheapPrepend);
        reader_index_ = kCheapPrepend;
        writer_index_ = reader_index_ + readable;
    }
}