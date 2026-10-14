#pragma once

#include <vector>
#include <string>

class Buffer
{
public:
    static const size_t kCheapPrepend=8;
    static const size_t kInitialSize=1024;

    explicit Buffer(size_t initial_size=kInitialSize);

    size_t readableBytes() const;
    size_t writableBytes() const;
    size_t prependableBytes() const;

    // 返回缓冲区中可读数据的起始地址
    const char* peek() const;

    // onMessage  Buffer -> string
    void retrieve(size_t len);
    void retrieveAll();

    // 把onMessage函数上报的Bufferr数据，转成string类型的数据返回
    std::string retrieveAllString();
    std::string retrieveAsString(size_t len);

    void ensureWritableBytes(size_t len);


    void append(const char* data,size_t len);

    // 从fd上读取数据
    ssize_t readFd(int fd,int* save_errno);
    // 向fd上发送数据
    ssize_t writeFd(int fd,int* save_errno);

private:
    char* begin();
    const char* begin() const;

    char* beginReader();
    const char* beginReader() const;

    char* beginWriter();
    const char* beginWriter() const;

    void makeSpace(size_t len);

    std::vector<char> buffer_;
    size_t reader_index_;
    size_t writer_index_;
} ;