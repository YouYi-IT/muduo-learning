#include <muduo/net/Socket.h>
#include <muduo/base/Logger.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>

#include <errno.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

namespace muduo::net
{

TcpConnection::TcpConnection(EventLoop* loop, const std::string& name, int sockfd, const InetAddress& local_addr,
                             const InetAddress& peer_addr)
    : loop_(loop),
      name_(name),
      state_(kConnecting),
      reading_(true),
      socket_(std::make_unique<Socket>(sockfd)),
      channel_(std::make_unique<Channel>(loop, sockfd)),
      local_addr_(local_addr),
      peer_addr_(peer_addr),
      highwater_mark_(64 * 1024 * 1024)  // 64M
{
    // 给channel设置相应的回调函数
    channel_->setReadCallback([this](const muduo::base::Timestamp& receive_time) { handleRead(receive_time); });
    channel_->setWriteCallback([this]() { handleWrite(); });
    channel_->setCloseCallback([this]() { handleClose(); });
    channel_->setErrorCallback([this]() { handleError(); });

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}
TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d", name_.c_str(), channel_->getFd(), static_cast<int>(state_));
}

EventLoop* TcpConnection::getLoop() const
{
    return loop_;
}
const std::string& TcpConnection::getName() const
{
    return name_;
}
const InetAddress& TcpConnection::localAddress() const
{
    return local_addr_;
}
bool TcpConnection::connected() const
{
    return state_ == kConnected;
}

void TcpConnection::setConnectionCallback(const ConnectionCallback cb)
{
    connection_callback_ = std::move(cb);
}
void TcpConnection::setMessageCallback(const MessageCallback cb)
{
    message_callback_ = std::move(cb);
}
void TcpConnection::setWriteCompleteCallback(const WriteCompleteCallback cb)
{
    write_complete_callback_ = std::move(cb);
}
void TcpConnection::setHighWaterMarkCallback(const HighWaterMarkCallback cb, size_t highwater_mark)
{
    highwater_mark_callback_ = std::move(cb);
    highwater_mark_ = highwater_mark;
}
void TcpConnection::setCloseCallback(const CloseCallback cb)
{
    close_callback_ = std::move(cb);
}

// 连接建立
void TcpConnection::connectEstablished()
{
    state_ = kConnected;
    // 相当于在这记录下channel_所属的TcpConnection，然后后面channel_执行事件回调时检查这个TcpConnection是否还存在
    channel_->tie(shared_from_this());
    channel_->enableReading();

    // 新连接建立，执行回调
    connection_callback_(shared_from_this());
}
// 连接销毁
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        state_ = kDisconnected;
        channel_->disableAll();
        connection_callback_(shared_from_this());
    }
    channel_->remove();
}

void TcpConnection::handleRead(const muduo::base::Timestamp& receive_time)
{
    int saved_errno = 0;
    ssize_t n = input_buffer_.readFd(channel_->getFd(), &saved_errno);
    if (n > 0)
    {
        // 已建立连接的用户有可读事件发生了，调用用户传入的回调操作onMessage
        // shared_from_this() 是 std::enable_shared_from_this 的成员函数，不是 std 命名空间下的自由函数
        message_callback_(shared_from_this(), &input_buffer_, receive_time);
    }
    else if (n == 0)
    {
        // 对端关闭（对端发送了FIN，长度为0）
        handleClose();
    }
    else
    {
        errno = saved_errno;
        LOG_ERROR("TcpConnection::handleRead error=%d", saved_errno);
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if (channel_->isWriting())
    {
        int saved_errno = 0;
        ssize_t n = output_buffer_.writeFd(channel_->getFd(), &saved_errno);
        if (n > 0)
        {
            output_buffer_.retrieve(n);
            if (output_buffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (write_complete_callback_)
                {
                    // 注意std::enable_shared_from_this用法，和lambda结合的用法
                    // 避免悬空指针：如果只捕获 this，当 lambda 真正执行时，this
                    // 指向的对象可能已经不存在，导致未定义行为 通过捕获 shared_from_this()，lambda 持有一个
                    // shared_ptr，将对象的生命周期延长到 lambda 执行完毕
                    loop_->queueInLoop([self = shared_from_this()]() { self->write_complete_callback_(self); });
                }
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite error=%d", saved_errno);
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing", channel_->getFd());
    }
}

void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d", channel_->getFd(), static_cast<int>(state_));
    state_ = kDisconnected;
    channel_->disableAll();

    // 执行用户注册的
    TcpConnectionPtr conn_ptr(shared_from_this());
    if (connection_callback_)
    {
        connection_callback_(conn_ptr);
    }
    if (close_callback_)
    {
        close_callback_(conn_ptr);
    }
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int saved_error = 0;
    if (::getsockopt(channel_->getFd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        saved_error = errno;
    }
    else
    {
        saved_error = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d", name_.c_str(), saved_error);
}

// 发送数据
void TcpConnection::send(const std::string& str)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(str.data(), str.size());
        }
        else
        {
            loop_->runInLoop([self = shared_from_this(), str]() { self->sendInLoop(str.data(), str.size()); });
        }
    }
}

// 应用写的快，内核发送数据慢，需要把待发送数据写入缓冲区，并且设置水位线，防止放入数据过多
void TcpConnection::sendInLoop(const void* message, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool fault_error = false;

    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing");
        return;
    }

    // channel_第一次开始写数据，而且缓冲区没有待发送数据
    if (!channel_->isWriting() && output_buffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->getFd(), message, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && write_complete_callback_)
            {
                // 既然这里数据全部发送完成，就不用再给channel设置epollout事件了
                loop_->queueInLoop([self = shared_from_this()]() { self->write_complete_callback_(self); });
            }
        }
        else
        {
            nwrote = 0;
            // 这个操作本来需要等待，但文件描述符/套接字被设成了非阻塞模式，所以内核不让你等
            // 而是立即返回失败，并设置 errno = EWOULDBLOCK，告诉你“现在还不能做，稍后再试”
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                // 连接已经断了，而且不是“暂时不可用”，而是对端关闭或连接被重置，不能再继续用这个 fd 读写了
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    fault_error = true;
                }
            }
        }
    }

    // 表明这次的write没有把数据全部发送出去，剩余的数据需要保存到缓冲区
    // 然后给channel注册epollout事件，poller发现tcp的发送缓冲区有空间，会持续通知相应的channel调用write_callback_
    if (!fault_error && remaining > 0)
    {
        size_t readable = output_buffer_.readableBytes();
        if (readable + remaining >= highwater_mark_ && readable < highwater_mark_ && highwater_mark_callback_)
        {
            loop_->queueInLoop([self = shared_from_this(), readable, remaining]()
                               { self->highwater_mark_callback_(self, readable + remaining); });
        }
        output_buffer_.append((char*)message + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }
}

// 关闭连接
void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        state_ = kDisconnecting;
        loop_->runInLoop([self = shared_from_this()]() { self->shutdownInLoop(); });
    }
}

void TcpConnection::shutdownInLoop()
{
    // output_buffer种没有要发送的数据，就关闭，还有数据的话就不用管，handleWrite里面写了当数据发送完后调用shutdownInLoop
    if (!channel_->isWriting())
    {
        socket_->shutdownWrite();  // 关闭写端，发送FIN，当对端也发送FIN，channel的revents就会有EPOLLHUP
    }
}

}  // namespace muduo::net